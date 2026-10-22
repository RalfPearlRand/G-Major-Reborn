// gmr_render — offline processing and A/B comparison tool.
//
//   gmr_render in.wav out.wav [--preset file.syx [--index N]] [--set key=value ...]
//   gmr_render --compare reference.wav candidate.wav
//   gmr_render --dump file.syx              (print every decoded parameter)
//
// --compare aligns the two files by cross-correlation and reports level, RMS error,
// correlation and 1/3-octave band differences: use it to compare the reconstruction
// against recordings of a real G-Major running the same preset.
#include "gmr/Engine.h"
#include "gmr/FactoryPresets.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace gmr;

struct Wav { int sr = 48000; std::vector<float> l, r; };

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static bool readWav(const char* path, Wav& w) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return false; }
    std::vector<uint8_t> d;
    uint8_t buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) { std::fprintf(stderr, "%s: not a WAV\n", path); return false; }
    int fmt = 0, ch = 0, bits = 0; size_t pos = 12; const uint8_t* data = nullptr; size_t dataLen = 0;
    while (pos + 8 <= d.size()) {
        const uint32_t len = rd32(&d[pos + 4]);
        if (!std::memcmp(&d[pos], "fmt ", 4)) {
            fmt = rd16(&d[pos + 8]); ch = rd16(&d[pos + 10]); w.sr = (int)rd32(&d[pos + 12]); bits = rd16(&d[pos + 22]);
            if (fmt == 0xFFFE && len >= 40) fmt = rd16(&d[pos + 32]);
        } else if (!std::memcmp(&d[pos], "data", 4)) { data = &d[pos + 8]; dataLen = std::min<size_t>(len, d.size() - pos - 8); }
        pos += 8 + len + (len & 1);
    }
    if (!data || ch < 1 || (fmt != 1 && fmt != 3)) { std::fprintf(stderr, "%s: unsupported WAV format\n", path); return false; }
    const int bps = bits / 8; const size_t frames = dataLen / (size_t)(bps * ch);
    w.l.resize(frames); w.r.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        for (int c = 0; c < std::min(ch, 2); ++c) {
            const uint8_t* p = data + (i * (size_t)ch + (size_t)c) * (size_t)bps;
            float v = 0;
            if (fmt == 3 && bits == 32) std::memcpy(&v, p, 4);
            else if (bits == 16) v = (int16_t)rd16(p) / 32768.0f;
            else if (bits == 24) { int32_t s = p[0] | (p[1] << 8) | (p[2] << 16); if (s & 0x800000) s |= ~0xFFFFFF; v = s / 8388608.0f; }
            else if (bits == 32) v = (int32_t)rd32(p) / 2147483648.0f;
            (c == 0 ? w.l : w.r)[i] = v;
        }
        if (ch == 1) w.r[i] = w.l[i];
    }
    return true;
}

static bool writeWav(const char* path, const Wav& w) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const uint32_t frames = (uint32_t)w.l.size(), dataLen = frames * 8;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataLen); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(3); w16(2); w32((uint32_t)w.sr); w32((uint32_t)w.sr * 8); w16(8); w16(32);
    std::fwrite("data", 1, 4, f); w32(dataLen);
    for (uint32_t i = 0; i < frames; ++i) { std::fwrite(&w.l[i], 4, 1, f); std::fwrite(&w.r[i], 4, 1, f); }
    std::fclose(f);
    return true;
}

static bool loadPreset(const char* path, int index, Patch& p) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::vector<uint8_t> d; uint8_t b[4096]; size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + n);
    std::fclose(f);
    size_t pos = 0;
    for (int i = 0; ; ++i) {
        DecodeResult r;
        Patch tmp;
        const bool ok = readPresetAt(d.data(), d.size(), pos, tmp, &r);
        if (!ok && pos >= d.size()) return false;
        if (i == index) {
            if (!r.checksumOk) std::fprintf(stderr, "warning: checksum mismatch in %s\n", path);
            p = tmp;
            return ok;
        }
    }
}

static void dump(const Patch& p) {
    std::printf("Name: \"%s\"  number %d\n", p.name.c_str(), p.number);
    for (int i = 0; i < pInLevel; ++i) {
        const ParamDef& d = paramDef(i);
        const int ap = algoParamFor(d.block);
        if (ap >= 0 && i != ap && !paramUsedBy(d, p.get(ap))) continue;
        char txt[64]; formatValue(i, p.get(i), txt, sizeof txt);
        const Modifier* m = p.modifierFor(i);
        std::printf("  %-14s %-12s %-14s", blockName(d.block), d.key, txt);
        if (m && m->source) std::printf("  <- M%d (%d/%d/%d)", m->source, m->minOut, m->midOut, m->maxOut);
        std::printf("\n");
    }
}

// ---------------------------------------------------------------- comparison
static void compare(const Wav& a, const Wav& b) {
    const size_t n = std::min(a.l.size(), b.l.size());
    if (n < 4096) { std::printf("files too short\n"); return; }
    // latency alignment: brute-force cross-correlation over +-50 ms on a 1 s window
    const int maxLag = a.sr / 20; const size_t win = std::min<size_t>(n - (size_t)maxLag * 2, (size_t)a.sr);
    int bestLag = 0; double best = -1e30;
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        double s = 0;
        for (size_t i = (size_t)maxLag; i < (size_t)maxLag + win; i += 2) s += (double)a.l[i] * b.l[(size_t)((long)i + lag)];
        if (s > best) { best = s; bestLag = lag; }
    }
    double ea = 0, eb = 0, ed = 0, xab = 0; size_t cnt = 0;
    for (size_t i = (size_t)maxLag; i + (size_t)maxLag < n; ++i) {
        const double x = a.l[i], y = b.l[(size_t)((long)i + bestLag)];
        ea += x * x; eb += y * y; ed += (x - y) * (x - y); xab += x * y; ++cnt;
    }
    std::printf("alignment lag      : %d samples (%.2f ms)\n", bestLag, 1000.0 * bestLag / a.sr);
    std::printf("level ref / cand   : %.2f dB / %.2f dB\n", 10 * std::log10(ea / cnt + 1e-20), 10 * std::log10(eb / cnt + 1e-20));
    std::printf("difference energy  : %.2f dB relative to reference\n", 10 * std::log10(ed / (ea + 1e-20) + 1e-20));
    std::printf("correlation        : %.4f\n", xab / std::sqrt(ea * eb + 1e-30));
    // 1/3-octave band levels via Goertzel-free band filtering (simple biquad band-pass)
    std::printf("1/3-octave band difference (candidate - reference):\n");
    for (double fc = 31.5; fc < std::min(16000.0, a.sr * 0.45); fc *= std::pow(2.0, 1.0 / 3.0)) {
        dsp::Biquad fa, fb;
        const float w0 = (float)(2 * M_PI * fc / a.sr), alpha = std::sin(w0) / (2 * 4.32f);
        const float a0 = 1 + alpha;
        dsp::BiquadCoefs c { alpha / a0, 0, -alpha / a0, -2 * std::cos(w0) / a0, (1 - alpha) / a0 };
        fa.c = c; fb.c = c;
        double sa = 0, sb = 0;
        for (size_t i = (size_t)maxLag; i + (size_t)maxLag < n; ++i) {
            const float x = fa.process(a.l[i]), y = fb.process(b.l[(size_t)((long)i + bestLag)]);
            sa += (double)x * x; sb += (double)y * y;
        }
        std::printf("  %7.1f Hz  %+6.2f dB\n", fc, 10 * std::log10((sb + 1e-20) / (sa + 1e-20)));
    }
}

static void sysexMap() {
    std::printf("| Offset (data / .syx) | Block | Key | Label | Algorithms | Range (stored) | Encoding |\n|---|---|---|---|---|---|---|\n");
    for (int i = 0; i < pInLevel; ++i) {
        const ParamDef& d = paramDef(i);
        std::string algos = "all";
        if (algoParamFor(d.block) >= 0 && d.algoMask != 0xFFFFFFFFu && i != algoParamFor(d.block)) {
            algos.clear();
            for (int a = 0; a < numAlgorithms(d.block); ++a) if (paramUsedBy(d, a)) { if (!algos.empty()) algos += ", "; algos += algorithmName(d.block, a); }
        }
        std::string range;
        char lo[64], hi[64]; formatValue(i, d.lo, lo, sizeof lo); formatValue(i, d.hi, hi, sizeof hi);
        range = std::to_string(d.lo) + ".." + std::to_string(d.hi) + " (" + lo + " .. " + hi + ")";
        const char* enc = d.enc == Enc::BoolInv ? "packed int, 0 = ON / 1 = OFF" : d.enc == Enc::Special ? "bit field @156" :
                          d.kind == Kind::Table ? "packed int = table index + offset" : "packed 24-bit signed int";
        std::printf("| %d / %d | %s | `%s` | %s | %s | %s | %s |\n", d.offset, d.offset + 1, blockName(d.block), d.key, d.label, algos.c_str(), range.c_str(), enc);
    }
    int n = 0; const ModSlot* ms = modSlots(n);
    std::printf("\n### Modifier slots\n\n| Offset | Block | Algorithm | Parameter |\n|---|---|---|---|\n");
    for (int i = 0; i < n; ++i) std::printf("| %d | %s | %s | `%s` |\n", ms[i].offset, blockName(ms[i].block), algorithmName(ms[i].block, ms[i].algo), paramDef(ms[i].param).key);
}

static int writeFactory(const char* dir) {
    int n = 0;
    for (const auto& p : factoryPresets()) {
        std::string fn = std::string(dir) + "/" + (p.numberInBank() < 10 ? "0" : "") + std::to_string(p.numberInBank()) + " " + p.name + ".syx";
        for (size_t k = std::strlen(dir) + 1; k < fn.size(); ++k) if (std::strchr("/\\:*?\"<>|()", fn[k])) fn[k] = '_';
        FILE* f = std::fopen(fn.c_str(), "wb");
        if (!f) { std::fprintf(stderr, "cannot write %s\n", fn.c_str()); return 1; }
        const auto b = encodeForFile(p); std::fwrite(b.data(), 1, b.size(), f); std::fclose(f); ++n;
    }
    std::printf("%d presets written to %s\n", n, dir);
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && !std::strcmp(argv[1], "--sysex-map")) { sysexMap(); return 0; }
    if (argc >= 3 && !std::strcmp(argv[1], "--write-factory")) return writeFactory(argv[2]);
    if (argc >= 3 && !std::strcmp(argv[1], "--dump")) {
        Patch p; if (!loadPreset(argv[2], argc > 3 ? std::atoi(argv[3]) : 0, p)) { std::fprintf(stderr, "no preset\n"); return 1; }
        dump(p); return 0;
    }
    if (argc >= 4 && !std::strcmp(argv[1], "--compare")) {
        Wav a, b; if (!readWav(argv[2], a) || !readWav(argv[3], b)) return 1;
        if (a.sr != b.sr) { std::fprintf(stderr, "sample rates differ\n"); return 1; }
        compare(a, b); return 0;
    }
    if (argc < 3) {
        std::printf("usage:\n  gmr_render in.wav out.wav [--preset f.syx [--index N]] [--set key=value ...] [--tail seconds]\n"
                    "  gmr_render --compare reference.wav candidate.wav\n  gmr_render --dump f.syx [index]\n"
                    "  gmr_render --sysex-map            (markdown table of the preset format)\n"
                    "  gmr_render --write-factory <dir>  (bundled presets as .syx files)\n");
        return 1;
    }
    Wav in; if (!readWav(argv[1], in)) return 1;
    Patch p; const char* preset = nullptr; int index = 0; double tail = 2.0;
    std::vector<std::pair<int, int>> sets;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--preset") && i + 1 < argc) preset = argv[++i];
        else if (!std::strcmp(argv[i], "--index") && i + 1 < argc) index = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--tail") && i + 1 < argc) tail = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--set") && i + 1 < argc) {
            std::string kv = argv[++i]; const size_t eq = kv.find('=');
            const int pi = paramIndexForKey(kv.substr(0, eq).c_str());
            if (pi < 0 || eq == std::string::npos) { std::fprintf(stderr, "unknown parameter %s\n", kv.c_str()); return 1; }
            sets.push_back({ pi, std::atoi(kv.c_str() + eq + 1) });
        }
    }
    if (preset && !loadPreset(preset, index, p)) { std::fprintf(stderr, "could not load preset %d from %s\n", index, preset); return 1; }
    Engine e; e.prepare(in.sr, 1024); e.loadPatch(p);
    for (auto& s : sets) e.setParam(s.first, s.second);
    e.reset();
    const size_t extra = (size_t)(tail * in.sr);
    in.l.resize(in.l.size() + extra, 0.0f); in.r.resize(in.r.size() + extra, 0.0f);
    for (size_t pos = 0; pos < in.l.size(); pos += 512) {
        const int n = (int)std::min<size_t>(512, in.l.size() - pos);
        e.process(in.l.data() + pos, in.r.data() + pos, n);
    }
    if (!writeWav(argv[2], in)) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
    std::printf("rendered %zu frames @ %d Hz with preset \"%s\"\n", in.l.size(), in.sr, p.name.c_str());
    return 0;
}
