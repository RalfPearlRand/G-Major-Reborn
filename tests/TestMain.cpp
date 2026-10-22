// G-Major Reborn — automated test suite (no external dependencies).
// Run: ctest --output-on-failure   or   ./gmr_tests [filter]
#include "gmr/Engine.h"
#include "gmr/Patch.h"
#include "gmr/FactoryPresets.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include <set>

using namespace gmr;

// ------------------------------------------------------------------ micro framework
struct TestCase { const char* name; std::function<void()> fn; };
static std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({ n, std::move(f) }); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
static int gFailures = 0, gChecks = 0;
#define CHECK(cond) do { ++gChecks; if (!(cond)) { ++gFailures; std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++gChecks; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (tol))) { ++gFailures; \
    std::printf("    FAIL %s:%d  %s = %.6g, expected %.6g (tol %.3g)\n", __FILE__, __LINE__, #a, _a, _b, (double)(tol)); } } while (0)

// ------------------------------------------------------------------ helpers
using Buf = std::vector<float>;
static Buf sine(double hz, double sr, int n, float amp = 0.5f) {
    Buf b((size_t)n);
    for (int i = 0; i < n; ++i) b[(size_t)i] = amp * (float)std::sin(2.0 * M_PI * hz * i / sr);
    return b;
}
static Buf noise(int n, float amp, unsigned seed) {
    std::mt19937 g(seed); std::uniform_real_distribution<float> d(-amp, amp);
    Buf b((size_t)n); for (auto& x : b) x = d(g); return b;
}
static double rms(const Buf& b, size_t from = 0, size_t to = 0) {
    if (to == 0 || to > b.size()) to = b.size();
    double s = 0; for (size_t i = from; i < to; ++i) s += (double)b[i] * b[i];
    return std::sqrt(s / (double)std::max<size_t>(1, to - from));
}
static double db(double x) { return 20.0 * std::log10(std::max(1e-12, x)); }
static double goertzel(const Buf& b, double hz, double sr, size_t from = 0) {
    const double w = 2.0 * M_PI * hz / sr, cw = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0; const size_t n = b.size() - from;
    for (size_t i = from; i < b.size(); ++i) { const double s = b[i] + cw * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt(s1 * s1 + s2 * s2 - cw * s1 * s2) * 2.0 / (double)n;
}
static bool allFinite(const Buf& b) { for (float x : b) if (!std::isfinite(x)) return false; return true; }
static float peak(const Buf& b) { float p = 0; for (float x : b) p = std::max(p, std::fabs(x)); return p; }

/** Engine with every block OFF and unity levels. */
static void neutral(Engine& e) {
    for (int p : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(p, 0);
    e.setParam(pOutLevel, 0); e.setParam(pRouting, 0);
}
static void render(Engine& e, Buf& l, Buf& r, int block = 256) {
    for (size_t pos = 0; pos < l.size(); pos += (size_t)block) {
        const int n = (int)std::min<size_t>((size_t)block, l.size() - pos);
        e.process(l.data() + pos, r.data() + pos, n);
    }
}

// ================================================================== tables & params
TEST(tables_storage_indices_match_glydian) {
    CHECK(tableStoredForLabel(tables::hiCut, "158.5") == 18);
    CHECK(tableStoredForLabel(tables::hiCut, "14.1k") == 57);
    CHECK(tableStoredForLabel(tables::hiCut, "1.00k") == 34);
    CHECK(tableStoredForLabel(tables::hiCut, "10.0k") == 54);
    CHECK(tableStoredForLabel(tables::hiCut, "2.00k") == 40);
    CHECK(tableStoredForLabel(tables::hiCut, "Off") == 60);
    CHECK(tableStoredForLabel(tables::loCut, "2.00k") == 40);
    CHECK(tableStoredForLabel(tables::eqFreq, "40.97") == 25);
    CHECK(tableStoredForLabel(tables::eqFreq, "Off") == 241);
    CHECK(tableStoredForLabel(tables::eqWidth, "0.2") == 3);
    CHECK(tableStoredForLabel(tables::eqWidth, "4.0") == 16);
    CHECK(tableStoredForLabel(tables::compTime, "1.0") == 3);
    CHECK(tableStoredForLabel(tables::compTime, "70") == 14);
    CHECK(tableStoredForLabel(tables::compTime, "50") == 13);
    CHECK(tableStoredForLabel(tables::compTime, "2000") == 23);
    CHECK(tableStoredForLabel(tables::compTime, "20") == 11);
    CHECK(tableStoredForLabel(tables::compTime, "1000") == 21);
    CHECK(tables::speed.size == 209);
    CHECK_NEAR(tableValue(tables::speed, 0), 0.05, 1e-6);
    CHECK_NEAR(tableValue(tables::speed, 208), 19.95, 1e-4);
    CHECK_NEAR(tableValue(tables::tapTempo, 6), 0.25, 1e-6);       // "1/4"
    CHECK_NEAR(tableValue(tables::tapTempo, 10), 0.125 * 2 / 3, 1e-6); // "1/8T"
}

TEST(param_table_consistency) {
    std::set<std::string> keys;
    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef& d = paramDef(i);
        CHECK(d.id == i);
        CHECK(keys.insert(d.key).second);
        CHECK(d.lo <= d.def && d.def <= d.hi);
        if (d.kind == Kind::Table) {
            CHECK(d.lo - d.table->storeOffset >= 0);
            CHECK(d.hi - d.table->storeOffset < d.table->size);
        }
        if (d.offset >= 0 && d.enc != Enc::Special) { CHECK(d.offset >= 160 && d.offset <= 608); CHECK(d.offset % 4 == 0); }
    }
    // no two parameters of the same block may share an offset under the same algorithm
    for (int i = 0; i < kNumParams; ++i)
        for (int j = i + 1; j < kNumParams; ++j) {
            const ParamDef &a = paramDef(i), &b = paramDef(j);
            if (a.offset < 0 || a.enc == Enc::Special || a.offset != b.offset) continue;
            const bool clash = a.block == b.block && (a.algoMask & b.algoMask) != 0;
            if (clash) std::printf("    clash %s / %s @%d\n", a.key, b.key, a.offset);
            CHECK(!clash);
        }
    int n = 0; const ModSlot* s = modSlots(n);
    for (int i = 0; i < n; ++i) {
        CHECK(s[i].offset >= 28 && s[i].offset <= 152 && s[i].offset % 4 == 0);
        CHECK(paramUsedBy(paramDef(s[i].param), s[i].algo));
        for (int j = i + 1; j < n; ++j)
            CHECK(!(s[i].block == s[j].block && s[i].algo == s[j].algo && s[i].offset == s[j].offset));
    }
}

// ================================================================== SysEx codec
TEST(packed24_roundtrip_and_glydian_bug) {
    uint8_t b[4];
    for (int v : { 0, 1, -1, 6, -6, 100, -100, 127, -127, 128, -128, -200, 200, 1800, -2400, 2400, 3000, 8388607, -8388608 }) {
        writePacked24(b, v);
        CHECK(readPacked24(b) == v);
        for (uint8_t x : b) CHECK(x < 0x80);
    }
    writePacked24(b, -6);  CHECK(b[0] == 122 && b[1] == 127 && b[2] == 127 && b[3] == 7);   // == GLydian writeInt
    for (int v = -127; v <= 3000; ++v) { if (v == -1) continue; writePacked24(b, v); CHECK(glydianReadInt(b) == v); }
    writePacked24(b, -1);
    CHECK(glydianReadInt(b) == 0);      // GLydian bug: -1 (7F 7F 7F 07) decodes as 0
    writePacked24(b, -200);
    CHECK(glydianReadInt(b) != -200);   // documented GLydian decoding bug for values <= -128
}

TEST(sysex_layout_header_checksum) {
    Patch p; p.name = "Test Patch"; p.number = 105;
    uint8_t m[kSysexSize]; encodeSysex(p, m);
    const uint8_t hdr[] = { 0xF0, 0x00, 0x20, 0x1F, 0x00, 0x48, 0x20 };
    CHECK(std::memcmp(m, hdr, sizeof(hdr)) == 0);
    CHECK(m[kSysexSize - 1] == 0xF7);
    CHECK(m[7] == 105 && m[8] == 0);
    CHECK(std::memcmp(m + 9, "Test Patch          ", 20) == 0);
    for (int i = 1; i < kSysexSize - 1; ++i) CHECK(m[i] < 0x80);
    CHECK(computeChecksum(m + 1) == m[613]);
    encodeSysex(p, m, true); CHECK(m[7] == 0 && m[8] == 0);        // edit buffer
    p.number = 150; encodeSysex(p, m); CHECK(m[7] == (150 & 0x7F) && m[8] == 1);
    const auto rq = makePresetRequest(150);
    const uint8_t want[] = { 0xF0, 0x00, 0x20, 0x1F, 0x00, 0x48, 0x45, 150 & 0x7F, 1, 0xF7 };
    CHECK(std::memcmp(rq.data(), want, 10) == 0);
    Patch u; u.number = Patch::absoluteNumber(Bank::User, 27);
    CHECK(u.number == 127 && u.bank() == Bank::User && u.numberInBank() == 27);
}

static Patch randomPatch(std::mt19937& g) {
    Patch p;
    for (int i = 0; i < pInLevel; ++i) {
        const ParamDef& d = paramDef(i);
        p.set(i, std::uniform_int_distribution<int>(d.lo, d.hi)(g));
    }
    for (auto& m : p.mods) {
        m.source = (uint8_t)std::uniform_int_distribution<int>(0, 4)(g);
        m.minOut = (uint8_t)std::uniform_int_distribution<int>(0, 100)(g);
        m.midOut = (uint8_t)std::uniform_int_distribution<int>(0, 100)(g);
        m.maxOut = (uint8_t)std::uniform_int_distribution<int>(0, 100)(g);
    }
    p.name = "Rnd" + std::to_string(g() % 1000);
    p.number = std::uniform_int_distribution<int>(0, 200)(g);
    return p;
}

TEST(sysex_roundtrip_fuzz_all_algorithms) {
    std::mt19937 g(1234);
    for (int it = 0; it < 2000; ++it) {
        Patch p = randomPatch(g);
        // full round trip through the file format (preset + optional extended-level message)
        const auto file = encodeForFile(p);
        size_t pos = 0; Patch q; DecodeResult r;
        CHECK(readPresetAt(file.data(), file.size(), pos, q, &r));
        CHECK(r.ok); CHECK(r.checksumOk); CHECK(pos == file.size());
        CHECK(q.name == p.name); CHECK(q.number == p.number);
        for (int i = 0; i < pInLevel; ++i) {
            const ParamDef& d = paramDef(i);
            const int ap = algoParamFor(d.block);
            if (ap >= 0 && i != ap && !paramUsedBy(d, p.get(ap))) continue;   // inactive algorithm field
            if (q.get(i) != p.get(i)) { std::printf("    %s %d != %d\n", d.key, q.get(i), p.get(i)); CHECK(false); }
        }
        for (int s = 0; s < kNumModSlots; ++s) CHECK(q.mods[(size_t)s] == p.mods[(size_t)s]);
        // the 615-byte message alone only carries values the hardware accepts
        Patch h; CHECK(decodeSysex(file.data(), kSysexSize, h).ok);
        for (int lv : { pOutLevel, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut }) {
            const int ap = algoParamFor(paramDef(lv).block);
            if (ap >= 0 && !paramUsedBy(paramDef(lv), p.get(ap))) continue;
            CHECK(h.get(lv) == std::clamp(p.get(lv), -50, 0));
        }
        CHECK(encodeExtension(p).empty() == (std::max({ p.get(pOutLevel), p.get(pFltOut), p.get(pPitOut), p.get(pModOut),
                                                        p.get(pDlyOut), p.get(pRevOut) }) <= 0));
        // re-encoding a decoded message reproduces it byte for byte
        uint8_t m[kSysexSize], m2[kSysexSize]; encodeSysex(p, m); encodeSysex(q, m2);
        CHECK(std::memcmp(m, m2, kSysexSize) == 0);
    }
}

TEST(extended_levels_range_and_hardware_compat) {
    CHECK(paramDef(pOutLevel).lo == -50 && paramDef(pOutLevel).hi == 50);
    for (int lv : { pFltOut, pPitOut, pModOut, pDlyOut, pRevOut, pInLevel, pGlobalOut })
        CHECK(paramDef(lv).lo == -50 && paramDef(lv).hi == 50);
    Patch p; p.set(pOutLevel, 12); p.set(pDlyOut, 50); p.set(pRevOut, -50);
    uint8_t m[kSysexSize]; encodeSysex(p, m);
    CHECK(readPacked24(m + 1 + 476) == 0);           // delay out clamped to the G-Major maximum
    CHECK(readPacked24(m + 1 + 536) == -50);
    const auto ext = encodeExtension(p);
    CHECK(!ext.empty() && ext[0] == 0xF0 && ext[1] == 0x7D && ext.back() == 0xF7);
    for (size_t i = 1; i + 1 < ext.size(); ++i) CHECK(ext[i] < 0x80);
    // a plain hardware dump (no extension) with "Off" (-100) maps to the new minimum
    Patch hw; hw.set(pDlyAlgo, 0); uint8_t h[kSysexSize]; encodeSysex(hw, h);
    writePacked24(h + 1 + 476, -100); h[1 + 612] = computeChecksum(h + 1);
    Patch q; CHECK(decodeSysex(h, kSysexSize, q).ok); CHECK(q.get(pDlyOut) == -50);
    // engine: +12 dB preset level really amplifies
    Engine e; e.prepare(48000, 512);
    for (int b : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(b, 0);
    e.setParam(pOutLevel, 12); e.reset();
    std::vector<float> l(4800, 0.1f), r = l; e.process(l.data(), r.data(), 4800);
    CHECK(std::fabs(l[4000] - 0.1f * std::pow(10.0f, 0.6f)) < 1e-4f);
}

TEST(sysex_preserves_unknown_bytes) {
    Patch p; uint8_t m[kSysexSize]; encodeSysex(p, m);
    m[1 + 190] = 0x55; m[1 + 606] = 0x11;          // bytes not owned by any known parameter
    m[1 + 612] = computeChecksum(m + 1);
    Patch q; CHECK(decodeSysex(m, kSysexSize, q).ok);
    uint8_t m2[kSysexSize]; encodeSysex(q, m2);
    CHECK(m2[1 + 190] == 0x55 && m2[1 + 606] == 0x11);
    CHECK(std::memcmp(m, m2, kSysexSize) == 0);
}

TEST(sysex_rejects_garbage_and_flags_checksum) {
    Patch q; uint8_t junk[kSysexSize] = {};
    CHECK(!decodeSysex(junk, kSysexSize, q).ok);
    Patch p; uint8_t m[kSysexSize]; encodeSysex(p, m);
    CHECK(!decodeSysex(m, 100, q).ok);
    m[1 + 300] ^= 0x01;
    const auto r = decodeSysex(m, kSysexSize, q);
    CHECK(r.ok && !r.checksumOk);
    std::vector<uint8_t> stream(50, 0x7E); stream.insert(stream.end(), m, m + kSysexSize);
    CHECK(findNextPreset(stream.data(), stream.size(), 0) == 50);
}

TEST(preset_file_save_and_reload) {
    std::mt19937 g(99);
    const char* path = "gmr_test_preset.syx";
    std::vector<Patch> bank;
    { FILE* f = std::fopen(path, "wb");
      for (int i = 0; i < 5; ++i) { bank.push_back(randomPatch(g)); const auto b = encodeForFile(bank.back()); std::fwrite(b.data(), 1, b.size(), f); }
      std::fclose(f); }
    std::vector<uint8_t> data;
    { FILE* f = std::fopen(path, "rb"); uint8_t buf[4096]; size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n); std::fclose(f); }
    std::remove(path);
    size_t pos = 0; int count = 0; Patch q;
    while (readPresetAt(data.data(), data.size(), pos, q)) {
        CHECK(q.name == bank[(size_t)count].name);
        CHECK(q.get(pDlyAlgo) == bank[(size_t)count].get(pDlyAlgo));
        CHECK(q.get(pOutLevel) == bank[(size_t)count].get(pOutLevel));
        ++count;
    }
    CHECK(count == 5);
}

// ================================================================== engine: basic signal tests
TEST(passthrough_all_blocks_off) {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        Engine e; e.prepare(sr, 512); neutral(e); e.reset();
        Buf l = noise(20000, 0.5f, 1), r = noise(20000, 0.5f, 2), l0 = l, r0 = r;
        render(e, l, r);
        double maxErr = 0; for (size_t i = 0; i < l.size(); ++i) maxErr = std::max<double>(maxErr, std::max(std::fabs(l[i] - l0[i]), std::fabs(r[i] - r0[i])));
        CHECK(maxErr < 1e-6);
    }
}

TEST(bypass_passes_input_unchanged) {
    Engine e; e.prepare(48000, 512);
    for (int p : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(p, 1);
    e.setParam(pBypass, 1); e.reset();
    Buf l = noise(9600, 0.5f, 3), r = noise(9600, 0.5f, 4), l0 = l;
    render(e, l, r);
    double maxErr = 0; for (size_t i = 0; i < l.size(); ++i) maxErr = std::max<double>(maxErr, std::fabs(l[i] - l0[i]));
    CHECK(maxErr < 1e-6);
}

TEST(levels_gain_and_off) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pOutLevel, -6); e.reset();
    Buf l = sine(1000, 48000, 9600), r = l; render(e, l, r);
    CHECK_NEAR(db(rms(l, 4800)) - db(rms(sine(1000, 48000, 9600), 4800)), -6.0, 0.05);
    e.setParam(pOutLevel, 0); e.setParam(pInLevel, -12); e.setParam(pGlobalOut, -3); e.reset();
    l = sine(1000, 48000, 9600); r = l; render(e, l, r);
    CHECK_NEAR(db(rms(l, 4800)) - db(rms(sine(1000, 48000, 9600), 4800)), -15.0, 0.05);
    e.setParam(pGlobalOut, -50); e.setParam(pInLevel, 0); e.reset();
    l = sine(1000, 48000, 9600); r = l; render(e, l, r);
    CHECK_NEAR(db(rms(l, 4800)) - db(rms(sine(1000, 48000, 9600), 4800)), -50.0, 0.05);
    e.setParam(pGlobalOut, 50); e.reset();
    l = sine(1000, 48000, 9600, 0.001f); r = l; render(e, l, r);
    CHECK_NEAR(db(rms(l, 4800)) - db(rms(sine(1000, 48000, 9600, 0.001f), 4800)), 50.0, 0.05);
}

TEST(no_internal_clipping_float_headroom) {
    Engine e; e.prepare(48000, 512); neutral(e); e.reset();
    Buf l = sine(500, 48000, 4800, 4.0f), r = l, l0 = l;   // +12 dBFS
    render(e, l, r);
    CHECK_NEAR(peak(l), peak(l0), 1e-5);
}

TEST(compressor_static_curve_and_limiter) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pCompOn, 1); e.setParam(pCompThreshold, -20); e.setParam(pCompRatio, 9 /*4.0:1*/);
    e.setParam(pCompAttack, 3); e.setParam(pCompRelease, 13); e.setParam(pCompGain, 0); e.reset();
    // steady sine whose peak is -8 dBFS -> 12 dB over -> 9 dB reduction -> -17 dBFS peak
    const float a = (float)std::pow(10.0, -8.0 / 20.0);
    Buf l = sine(200, 48000, 48000, a), r = l; render(e, l, r);
    float pk = 0; for (size_t i = 24000; i < l.size(); ++i) pk = std::max(pk, std::fabs(l[i]));
    CHECK_NEAR(db(pk), -17.0, 1.0);
    e.setParam(pCompRatio, 15 /*Inf*/); e.reset();
    l = sine(200, 48000, 48000, 1.0f); r = l; render(e, l, r);
    pk = 0; for (size_t i = 24000; i < l.size(); ++i) pk = std::max(pk, std::fabs(l[i]));
    CHECK(db(pk) < -19.0 && db(pk) > -21.5);
}

TEST(eq_frequency_response) {
    for (double sr : { 44100.0, 96000.0 }) {
        Engine e; e.prepare(sr, 512); neutral(e);
        e.setParam(pEqOn, 1);
        e.setParam(pEq1Freq, tableStoredForLabel(tables::eqFreq, "1.00k")); e.setParam(pEq1Gain, 12);
        e.setParam(pEq1Width, tableStoredForLabel(tables::eqWidth, "1.0"));
        e.setParam(pEq2Gain, 0); e.setParam(pEq3Gain, 0); e.reset();
        auto gainAt = [&](double hz) { Buf l = sine(hz, sr, (int)sr), r = l; e.reset(); render(e, l, r);
            return db(goertzel(l, hz, sr, (size_t)(sr / 2))) - db(0.5); };
        CHECK_NEAR(gainAt(1000), 12.0, 0.3);
        CHECK_NEAR(gainAt(100), 0.0, 0.5);
        CHECK_NEAR(gainAt(10000), 0.0, 0.6);
        CHECK(gainAt(1414) > 4.0 && gainAt(1414) < 9.0);   // half an octave away, inside the bell
    }
}

TEST(resonance_filter_lowpass_slopes) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pFltOn, 1); e.setParam(pFltAlgo, algo::Resonance); e.setParam(pFltMix, 100); e.setParam(pFltOut, 0);
    e.setParam(pFltResHiCut, tableStoredForLabel(tables::hiCut, "1.00k")); e.setParam(pFltResonance, 0);
    auto gainAt = [&](double hz) { Buf l = sine(hz, 48000, 48000), r = l; e.reset(); render(e, l, r);
        return db(goertzel(l, hz, 48000, 24000)) - db(0.5); };
    e.setParam(pFltOrder, 0);
    CHECK_NEAR(gainAt(100), 0.0, 0.5);
    const double g2 = gainAt(8000);
    CHECK(g2 < -30.0 && g2 > -42.0);                    // 12 dB/oct * 3 oct = -36
    e.setParam(pFltOrder, 1);
    CHECK(gainAt(8000) < -60.0);                        // 24 dB/oct
    e.setParam(pFltOrder, 0); e.setParam(pFltResonance, 100);
    CHECK(gainAt(1000) > 12.0);                         // resonant peak
}

TEST(delay_echo_timing_all_rates) {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        Engine e; e.prepare(sr, 512); neutral(e);
        e.setParam(pDlyOn, 1); e.setParam(pDlyAlgo, algo::Dual); e.setParam(pDlyMix, 100); e.setParam(pDlyOut, 0);
        e.setParam(pDlyTime, 300); e.setParam(pDlyTime2, 300); e.setParam(pDlyFeedback, 0); e.setParam(pDlyFeedback2, 0);
        e.setParam(pDlyFbHiCut, 60); e.setParam(pDlyFbLoCut, 0); e.reset();
        Buf l((size_t)sr, 0.0f), r = l; l[100] = r[100] = 1.0f;
        render(e, l, r);
        size_t arg = 0; float best = 0; for (size_t i = 0; i < l.size(); ++i) if (std::fabs(l[i]) > best) { best = std::fabs(l[i]); arg = i; }
        CHECK_NEAR((double)arg, 100.0 + 0.3 * sr, 2.0);
    }
}

TEST(delay_tempo_sync_tap_and_host) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pDlyOn, 1); e.setParam(pDlyAlgo, algo::Dual); e.setParam(pDlyMix, 100);
    e.setParam(pDlyTime, 100); e.setParam(pDlyTempo, 6 /* 1/4 */); e.setParam(pDlyFeedback, 0);
    e.setParam(pDlyTime2, 0); e.setParam(pDlyFbHiCut, 60); e.setParam(pDlyFbLoCut, 0);
    auto echoAt = [&]() { e.reset(); Buf l(48000, 0.0f), r = l; l[0] = r[0] = 1.0f; render(e, l, r);
        size_t a = 0; float b = 0; for (size_t i = 1000; i < l.size(); ++i) if (std::fabs(l[i]) > b) { b = std::fabs(l[i]); a = i; } return (double)a; };
    e.setParam(pTapTempo, 400);                          // tapped quarter = 400 ms
    CHECK_NEAR(echoAt(), 0.4 * 48000, 2.0);
    e.setParam(pTempoSource, 1); e.setHostTempo(120.0, true);   // host 120 BPM -> 500 ms
    CHECK_NEAR(echoAt(), 0.5 * 48000, 2.0);
}

TEST(kill_dry_removes_direct_signal) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pDlyOn, 1); e.setParam(pDlyMix, 50); e.setParam(pDlyTime, 500); e.setParam(pKillDry, 1); e.reset();
    Buf l = sine(440, 48000, 9600), r = l; render(e, l, r);
    CHECK(rms(l, 0, 20000) < 1e-4);                      // before the first echo: silence
    e.setParam(pKillDry, 0); e.reset();
    l = sine(440, 48000, 9600); r = l; render(e, l, r);
    CHECK_NEAR(db(rms(l, 2000, 9600)), db(rms(sine(440, 48000, 9600), 2000)) - 6.02, 0.1);  // 50 % dry
}

TEST(parallel_routing_dry_at_unity) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pRouting, 2); e.setParam(pDlyOn, 1); e.setParam(pDlyMix, 100); e.setParam(pDlyTime, 500);
    e.setParam(pRevOn, 1); e.setParam(pRevMix, 0); e.setParam(pRevPreDelay, 100); e.reset();
    Buf l = sine(440, 48000, 9600), r = l, l0 = l; render(e, l, r);
    double err = 0; for (size_t i = 0; i < 9600; ++i) err = std::max<double>(err, std::fabs(l[i] - l0[i]));
    CHECK(err < 1e-4);                                   // dry once, delay not yet, reverb mix 0
}

TEST(reverb_decay_tracks_decay_parameter) {
    for (int algoIdx = 0; algoIdx < algo::ReverbCount; ++algoIdx) {
        Engine e; e.prepare(48000, 512); neutral(e);
        e.setParam(pRevOn, 1); e.setParam(pRevAlgo, algoIdx); e.setParam(pRevMix, 100); e.setParam(pRevPreDelay, 0);
        e.setParam(pRevLevel, -100); e.setParam(pRevRoomLevel, 0); e.setParam(pRevDecay, 15 /*1.5 s*/); e.reset();
        Buf l(48000 * 3, 0.0f), r = l; l[0] = r[0] = 1.0f; render(e, l, r);
        CHECK(allFinite(l));
        const double e1 = db(rms(l, 4800, 14400)), e2 = db(rms(l, 4800 + 48000, 14400 + 48000));
        const double perSec = e1 - e2;                    // ideal: 40 dB per second for RT60 = 1.5 s
        if (perSec < 20.0 || perSec > 70.0) std::printf("    algo %d decay %.1f dB/s\n", algoIdx, perSec);
        CHECK(perSec > 20.0 && perSec < 70.0);
        CHECK(e1 > -60.0);                                // audible tail
    }
}

TEST(pitch_octaver_shifts_one_octave) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pPitOn, 1); e.setParam(pPitAlgo, algo::Octaver); e.setParam(pPitMix, 100); e.setParam(pPitOut, 0);
    e.setParam(pPitDirection, 1); e.setParam(pPitRange, 1); e.reset();
    Buf l = sine(220, 48000, 48000), r = l; render(e, l, r);
    const double f440 = goertzel(l, 440, 48000, 12000), f220 = goertzel(l, 220, 48000, 12000);
    CHECK(db(f440) - db(f220) > 15.0);
    e.setParam(pPitDirection, 0); e.reset();
    l = sine(440, 48000, 48000); r = l; render(e, l, r);
    CHECK(db(goertzel(l, 220, 48000, 12000)) - db(goertzel(l, 440, 48000, 12000)) > 15.0);
}

TEST(pitch_shifter_voice_interval_and_whammy_modifier) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pPitOn, 1); e.setParam(pPitAlgo, algo::Shifter); e.setParam(pPitMix, 100);
    e.setParam(pShfVoice1, 700); e.setParam(pShfLevel1, 0); e.setParam(pShfLevel2, -100);
    e.setParam(pShfDelay1, 0); e.setParam(pShfFb1, 0); e.setParam(pShfPan1, 0); e.reset();
    Buf l = sine(300, 48000, 48000), r = l; render(e, l, r);
    const double fifth = 300.0 * std::pow(2.0, 7.0 / 12.0);
    CHECK(db(goertzel(l, fifth, 48000, 12000)) - db(goertzel(l, 300, 48000, 12000)) > 15.0);

    // Whammy driven by modifier M1 (expression pedal) through the Pitch parameter
    neutral(e);
    e.setParam(pPitOn, 1); e.setParam(pPitAlgo, algo::Whammy); e.setParam(pPitDirection, 1); e.setParam(pPitRange, 1);
    e.setParam(pWhamPitch, 0);
    const int slot = (modifierOffset(pWhamPitch, algo::Whammy) - kFirstModOffset) / 4;
    e.setModifier(slot, Modifier{ 1, 0, 50, 100 });
    e.setParam(pMod1, 127); e.reset();
    l = sine(220, 48000, 48000); r = l; render(e, l, r);
    CHECK_NEAR(e.effective(pWhamPitch), 100.0, 1e-3);
    CHECK(db(goertzel(l, 440, 48000, 12000)) - db(goertzel(l, 220, 48000, 12000)) > 15.0);
    e.setParam(pMod1, 0);
    l = sine(220, 48000, 4800); r = l; render(e, l, r);
    CHECK_NEAR(e.effective(pWhamPitch), 0.0, 1e-3);
}

TEST(modifier_curve_shape) {
    const Modifier m{ 1, 10, 50, 90 };
    CHECK_NEAR(modifierCurve(m, 0.0f), 10.0, 1e-4);
    CHECK_NEAR(modifierCurve(m, 0.5f), 50.0, 1e-4);
    CHECK_NEAR(modifierCurve(m, 1.0f), 90.0, 1e-4);
    const Modifier inv{ 1, 100, 50, 0 };
    CHECK_NEAR(modifierCurve(inv, 1.0f), 0.0, 1e-4);
}

TEST(noise_gate_attenuates_below_threshold) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pGateOn, 1); e.setParam(pGateMode, 1); e.setParam(pGateThreshold, -30);
    e.setParam(pGateMaxDamp, 40); e.setParam(pGateRelease, 200); e.reset();
    Buf l = noise(48000, 0.003f, 5), r = l;                      // ~ -50 dBFS
    const double in = rms(l, 24000); render(e, l, r);
    CHECK_NEAR(db(rms(l, 24000)) - db(in), -40.0, 1.0);
    l = sine(440, 48000, 48000, 0.5f); r = l; render(e, l, r);  // loud signal re-opens the gate
    CHECK_NEAR(db(rms(l, 24000)), db(rms(sine(440, 48000, 48000, 0.5f), 24000)), 0.3);
}

TEST(tremolo_depth_full_mute) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pFltOn, 1); e.setParam(pFltAlgo, algo::Tremolo); e.setParam(pFltOut, 0);
    e.setParam(pFltDepth, 100); e.setParam(pFltTremType, 1); e.setParam(pFltPulseWidth, 50);
    e.setParam(pFltSpeed, tableStoredForLabel(tables::speed, "2.00")); e.setParam(pFltTempo, 0); e.setParam(pFltTremHiCut, 60); e.reset();
    Buf l(48000, 0.5f), r = l; render(e, l, r);
    float mn = 1, mx = 0; for (size_t i = 4800; i < l.size(); ++i) { mn = std::min(mn, std::fabs(l[i])); mx = std::max(mx, std::fabs(l[i])); }
    CHECK(mn < 1e-3); CHECK_NEAR(mx, 0.5, 0.01);
}

// ================================================================== robustness
TEST(stability_fuzz_every_algorithm_every_rate) {
    std::mt19937 g(777);
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        for (int it = 0; it < 24; ++it) {
            Engine e; e.prepare(sr, 1024);
            Patch p = randomPatch(g);
            for (int b : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) p.set(b, 1);
            p.set(pOutLevel, 0);
            for (int lv : { pFltOut, pPitOut, pModOut, pDlyOut, pRevOut }) p.set(lv, std::min(p.get(lv), 0));  // stability, not gain
            e.loadPatch(p); e.setParam(pMod1, (int)(g() % 128)); e.setParam(pMod2, (int)(g() % 128));
            e.reset();
            Buf l = noise((int)sr * 2, 0.7f, (unsigned)it), r = noise((int)sr * 2, 0.7f, (unsigned)it + 100);
            for (size_t i = 0; i < l.size(); i += 4000) l[i] = r[i] = 1.0f;   // impulses
            render(e, l, r, 333);
            CHECK(allFinite(l)); CHECK(allFinite(r));
            CHECK(peak(l) < 32.0f && peak(r) < 32.0f);
            if (peak(l) >= 32.0f && std::getenv("GMR_DEBUG")) {
                std::printf("    sr %.0f it %d peak %.1f routing %d\n", sr, it, peak(l), p.get(pRouting));
                for (int b : { pFltAlgo, pPitAlgo, pModAlgo, pDlyAlgo, pRevAlgo }) std::printf("      %s=%d", paramDef(b).key, p.get(b));
                std::printf("\n      comp thr %d ratio %d gain %d | eq gains %d %d %d | dly fb %d fb2 %d | mod fb %d | shf fb %d %d | flt fb %d reso %d | killdry %d\n",
                    p.get(pCompThreshold), p.get(pCompRatio), p.get(pCompGain), p.get(pEq1Gain), p.get(pEq2Gain), p.get(pEq3Gain),
                    p.get(pDlyFeedback), p.get(pDlyFeedback2), p.get(pModFeedback), p.get(pShfFb1), p.get(pShfFb2), p.get(pFltFeedback), p.get(pFltResonance), p.get(pKillDry));
                for (int lv : { pFltOut, pPitOut, pModOut, pDlyOut, pRevOut, pShfLevel1, pShfLevel2, pRevRoomLevel, pRevLevel }) std::printf("      %s=%d", paramDef(lv).key, p.get(lv));
                std::printf("\n");
            }
            CHECK(e.resetsAfterNonFinite() == 0);
            Buf sl((size_t)sr, 0.0f), sr2 = sl; render(e, sl, sr2, 512);   // tails must die / stay bounded
            CHECK(allFinite(sl) && peak(sl) < 32.0f);
        }
    }
}

TEST(modifier_on_out_level_uses_hardware_range) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pDlyOn, 1); e.setParam(pDlyAlgo, algo::PingPong); e.setParam(pDlyOut, 40);
    const int slot = (modifierOffset(pDlyOut, algo::PingPong) - kFirstModOffset) / 4;
    e.setModifier(slot, Modifier { 1, 0, 50, 100 });
    Buf l(512, 0.0f), r = l;
    e.setParam(pMod1, 127); e.process(l.data(), r.data(), 512);
    CHECK_NEAR(e.effective(pDlyOut), 0.0, 1e-4);          // pedal max = 0 dB, never +40/+50
    e.setParam(pMod1, 0); e.process(l.data(), r.data(), 512);
    CHECK_NEAR(e.effective(pDlyOut), -100.0, 1e-4);       // pedal min = Off (like the hardware)
}

TEST(maximum_gain_stack_stays_finite) {
    Engine e; e.prepare(48000, 512);
    for (int b : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(b, 1);
    for (int lv : { pOutLevel, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut, pInLevel, pGlobalOut }) e.setParam(lv, 50);
    e.reset();
    Buf l = noise(48000, 0.5f, 21), r = l; render(e, l, r);
    CHECK(allFinite(l) && allFinite(r));
    CHECK(e.resetsAfterNonFinite() == 0);
}

TEST(denormal_and_silence_safety) {
    Engine e; e.prepare(48000, 512);
    for (int p : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(p, 1);
    e.reset();
    Buf l(48000 * 4, 1e-30f), r = l;
    render(e, l, r);
    CHECK(allFinite(l));
    for (float x : l) CHECK(std::fpclassify(x) != FP_SUBNORMAL);
}

TEST(nan_input_is_contained) {
    Engine e; e.prepare(48000, 512); neutral(e); e.setParam(pRevOn, 1); e.setParam(pDlyOn, 1); e.reset();
    Buf l = sine(440, 48000, 4800), r = l; l[1000] = std::nanf("");
    render(e, l, r);
    CHECK(allFinite(l) && allFinite(r));
    CHECK(e.resetsAfterNonFinite() >= 1);
    Buf l2 = sine(440, 48000, 48000), r2 = l2; render(e, l2, r2);  // recovers afterwards
    CHECK(allFinite(l2) && rms(l2, 24000) > 0.1);
}

TEST(parameter_changes_during_playback) {
    std::mt19937 g(4242);
    Engine e; e.prepare(48000, 512);
    for (int p : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) e.setParam(p, 1);
    e.reset();
    Buf l = noise(48000 * 10, 0.5f, 8), r = noise(48000 * 10, 0.5f, 9);
    for (size_t pos = 0; pos < l.size(); pos += 256) {
        for (int k = 0; k < 6; ++k) {                       // hammer random parameters, incl. algorithms & routing
            const int p = (int)(g() % (unsigned)pInLevel);
            const ParamDef& d = paramDef(p);
            const int hi = isExtendedLevel(p) ? 0 : d.hi;     // gain is tested separately
            e.setParam(p, std::uniform_int_distribution<int>(d.lo, hi)(g));
        }
        e.setParam(pMod1, (int)(g() % 128));
        e.process(l.data() + pos, r.data() + pos, 256);
    }
    CHECK(allFinite(l) && allFinite(r));
    CHECK(peak(l) < 32.0f);
    CHECK(e.resetsAfterNonFinite() == 0);
}

TEST(buffer_size_independence) {
    auto run = [](int block) {
        Engine e; e.prepare(48000, 4096);
        Patch p;   // defaults: all blocks with musical settings
        for (int b : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) p.set(b, 1);
        e.loadPatch(p); e.reset();
        Buf l = noise(48000, 0.5f, 11), r = noise(48000, 0.5f, 12);
        render(e, l, r, block);
        return l;
    };
    const Buf ref = run(32);
    for (int bs : { 1, 7, 64, 100, 512, 4096 }) {
        const Buf o = run(bs);
        double err = 0; for (size_t i = 0; i < o.size(); ++i) err = std::max<double>(err, std::fabs(o[i] - ref[i]));
        if (err > 1e-3) std::printf("    block %d max diff %.3g\n", bs, err);
        CHECK(err < 1e-3);
    }
}

TEST(block_toggle_is_click_free) {
    Engine e; e.prepare(48000, 512); neutral(e);
    e.setParam(pFltAlgo, algo::Resonance); e.setParam(pFltMix, 100); e.reset();
    Buf l = sine(200, 48000, 48000, 0.5f), r = l;
    double maxStep = 0; float prev = 0;
    for (size_t pos = 0; pos < l.size(); pos += 128) {
        e.setParam(pFltOn, (pos / 2400) % 2 ? 1 : 0);
        e.process(l.data() + pos, r.data() + pos, 128);
        for (size_t i = pos; i < pos + 128; ++i) { maxStep = std::max<double>(maxStep, std::fabs(l[i] - prev)); prev = l[i]; }
    }
    const double sineStep = 0.5 * 2 * M_PI * 200 / 48000;     // max slope of the dry sine
    CHECK(maxStep < sineStep * 3.0);
}

TEST(factory_presets_valid_and_roundtrip) {
    CHECK(factoryPresetLabelErrors() == 0);
    const auto presets = factoryPresets();
    CHECK(presets.size() >= 16);
    std::set<int> numbers;
    for (const Patch& p : presets) {
        CHECK(numbers.insert(p.number).second);
        CHECK(p.bank() == Bank::User);
        CHECK(p.name.size() <= 20);
        uint8_t m[kSysexSize]; encodeSysex(p, m);
        Patch q; CHECK(decodeSysex(m, kSysexSize, q).ok);
        CHECK(q.name == p.name);
        Engine e; e.prepare(48000, 512); e.loadPatch(q); e.reset();
        Buf l = noise(48000, 0.3f, 3), r = noise(48000, 0.3f, 4);
        render(e, l, r);
        CHECK(allFinite(l) && peak(l) < 8.0f && rms(l) > 1e-3);
    }
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (auto& t : registry()) {
        if (filter && !std::strstr(t.name, filter)) continue;
        const int before = gFailures;
        std::printf("[ RUN  ] %s\n", t.name); std::fflush(stdout);
        t.fn();
        ++run;
        if (gFailures != before) { ++failedTests; std::printf("[ FAIL ] %s\n", t.name); }
        else std::printf("[  OK  ] %s\n", t.name);
    }
    std::printf("\n%d tests, %d checks, %d failed checks, %d failed tests\n", run, gChecks, gFailures, failedTests);
    return failedTests == 0 ? 0 : 1;
}
