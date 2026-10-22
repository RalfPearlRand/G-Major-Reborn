#include "gmr/Patch.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace gmr {

static const uint8_t kHeader[6] = { 0x00, 0x20, 0x1F, 0x00, 0x48, 0x20 };

// ------------------------------------------------------------------ packed integers
int32_t readPacked24(const uint8_t* p) noexcept {
    uint32_t u = (uint32_t)(p[0] & 0x7F) | ((uint32_t)(p[1] & 0x7F) << 7)
               | ((uint32_t)(p[2] & 0x7F) << 14) | ((uint32_t)(p[3] & 0x07) << 21);
    if (u & 0x800000u) u |= 0xFF000000u;   // sign-extend 24-bit
    return (int32_t)u;
}

void writePacked24(uint8_t* p, int32_t value) noexcept {
    const uint32_t u = (uint32_t)value & 0xFFFFFFu;
    p[0] = (uint8_t)(u & 0x7F);
    p[1] = (uint8_t)((u >> 7) & 0x7F);
    p[2] = (uint8_t)((u >> 14) & 0x7F);
    p[3] = (uint8_t)((u >> 21) & 0x07);
}

int32_t glydianReadInt(const uint8_t* p) noexcept {
    // Literal port of GmajorPatch.readInt(pData, off, 4)
    int v = 0;
    const bool negative = p[3] == 7;
    for (int i = 0; i < 3; ++i) {
        int bv = (int8_t)p[i];
        if (negative) bv = (bv != 127) ? bv - 128 : 0;
        v += bv * (1 << (i * 7));
    }
    return v;
}

uint8_t computeChecksum(const uint8_t* d) noexcept {
    unsigned s = 0;
    for (int i = 28; i < 612; ++i) s += d[i];
    return (uint8_t)(s & 0x7F);
}

// ------------------------------------------------------------------ Patch
Patch::Patch() {
    for (int i = 0; i < kNumParams; ++i) v[(size_t)i] = paramDef(i).def;
}

void Patch::set(int p, int stored) noexcept {
    if (p < 0 || p >= kNumParams) return;
    const ParamDef& d = paramDef(p);
    v[(size_t)p] = std::clamp(stored, d.lo, d.hi);
}

const Modifier* Patch::modifierFor(int param) const noexcept {
    const ParamDef& d = paramDef(param);
    const int off = modifierOffset(param, algo(d.block));
    if (off < kFirstModOffset) return nullptr;
    return &mods[(size_t)((off - kFirstModOffset) / 4)];
}

Modifier* Patch::modifierFor(int param) noexcept {
    return const_cast<Modifier*>(static_cast<const Patch*>(this)->modifierFor(param));
}

Bank Patch::bank() const noexcept {
    if (number <= 0) return Bank::EditBuffer;
    return number <= 100 ? Bank::Factory : Bank::User;
}
int Patch::numberInBank() const noexcept {
    if (number <= 0) return 0;
    return number <= 100 ? number : number - 100;
}
int Patch::absoluteNumber(Bank b, int n) noexcept {
    n = std::clamp(n, 1, 100);
    switch (b) { case Bank::Factory: return n; case Bank::User: return 100 + n; default: return 0; }
}

// ------------------------------------------------------------------ helpers
namespace {

uint32_t readBits28(const uint8_t* p) noexcept {
    return (uint32_t)(p[0] & 0x7F) | ((uint32_t)(p[1] & 0x7F) << 7)
         | ((uint32_t)(p[2] & 0x7F) << 14) | ((uint32_t)(p[3] & 0x7F) << 21);
}
void writeBits28(uint8_t* p, uint32_t u) noexcept {
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)((u >> (7 * i)) & 0x7F);
}
uint32_t bits(uint32_t u, int off, int n) noexcept { return (u >> off) & ((1u << n) - 1u); }
uint32_t setBits(uint32_t u, int off, int n, uint32_t val) noexcept {
    const uint32_t m = ((1u << n) - 1u) << off;
    return (u & ~m) | ((val << off) & m);
}

// Bit field at 156 (GmajorPatch.initialize/createSysex):
//   routing bits 0-1, outLevel+100 bits 2-8, relay1, relay2.
// GLydian READS the relays at bits 9/10 but WRITES them at 10/11 (internal inconsistency).
// We read and write at 9/10 (adjacent to the out level, matches the reader). UNVERIFIED on hardware.
constexpr int kRelay1Bit = 9;
constexpr int kRelay2Bit = 10;

Modifier unpackModifier(const uint8_t* p) noexcept {
    const uint32_t u = readBits28(p);
    Modifier m;
    m.source = (uint8_t)std::min<uint32_t>(bits(u, 0, 3), 4);
    m.minOut = (uint8_t)std::min<uint32_t>(bits(u, 3, 7), 100);
    m.midOut = (uint8_t)std::min<uint32_t>(bits(u, 10, 7), 100);
    m.maxOut = (uint8_t)std::min<uint32_t>(bits(u, 17, 7), 100);
    return m;
}
void packModifier(uint8_t* p, const Modifier& m) noexcept {
    uint32_t u = readBits28(p);            // keep bits 24..27 (unknown)
    u = setBits(u, 0, 3, m.source);
    u = setBits(u, 3, 7, m.minOut);
    u = setBits(u, 10, 7, m.midOut);
    u = setBits(u, 17, 7, m.maxOut);
    writeBits28(p, u);
}

bool algoAware(Block b) { return algoParamFor(b) >= 0; }

} // namespace

// ------------------------------------------------------------------ decode / encode
DecodeResult decodeSysex(const uint8_t* msg, size_t len, Patch& patch) {
    DecodeResult r;
    if (len < (size_t)kSysexSize || msg[0] != 0xF0 || msg[kSysexSize - 1] != 0xF7) {
        r.error = "Not a 615-byte SysEx message";
        return r;
    }
    const uint8_t* d = msg + 1;
    if (d[0] != kHeader[0] || d[1] != kHeader[1] || d[2] != kHeader[2] || d[4] != kHeader[4]) {
        r.error = "Not a TC Electronic G-Major message";
        return r;
    }
    if (d[5] != kHeader[5]) { r.error = "Not a preset-data message"; return r; }

    Patch p;  // start from defaults so params of inactive algorithms are sane
    std::memcpy(p.raw.data(), msg, kSysexSize);
    p.hasRaw = true;
    p.number = d[6] + 128 * d[7];

    char name[21] = {};
    for (int i = 0; i < 20; ++i) { const uint8_t c = d[8 + i]; name[i] = (c >= 32 && c < 127) ? (char)c : ' '; }
    std::string s(name);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    p.name = s;

    for (int i = 0; i < kNumModSlots; ++i) p.mods[(size_t)i] = unpackModifier(d + kFirstModOffset + 4 * i);

    const uint32_t bf = readBits28(d + 156);
    p.set(pRouting, (int)bits(bf, 0, 2));
    p.set(pOutLevel, (int)bits(bf, 2, 7) - 100);
    p.set(pRelay1, (int)bits(bf, kRelay1Bit, 1));
    p.set(pRelay2, (int)bits(bf, kRelay2Bit, 1));

    // algorithm selectors first, so shared offsets are decoded for the right algorithm
    for (Block b : { Block::Filter, Block::Pitch, Block::Mod, Block::Delay, Block::Reverb }) {
        const int ap = algoParamFor(b);
        int a = readPacked24(d + paramDef(ap).offset);
        if (b == Block::Delay && a == 12) a = algo::Dual;     // GLydian accepts 12 as DUAL
        p.set(ap, a);
    }

    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef& def = paramDef(i);
        if (def.offset < 0 || def.enc == Enc::Special || def.enc == Enc::None) continue;
        if (i == algoParamFor(def.block)) continue;
        if (algoAware(def.block) && !paramUsedBy(def, p.algo(def.block))) continue;
        int raw = readPacked24(d + def.offset);
        if (def.enc == Enc::BoolInv) raw = (raw == 0) ? 1 : 0;
        if (i == pPitRange && raw < 1) raw = 1;
        p.set(i, raw);
    }
    // keep live/unit settings of the current instance
    for (int i = pInLevel; i < kNumParams; ++i) p.v[(size_t)i] = patch.v[(size_t)i];

    r.checksumOk = computeChecksum(d) == d[612];
    r.ok = true;
    patch = p;
    return r;
}

void encodeSysex(const Patch& p, uint8_t* out, bool asEditBuffer) {
    if (p.hasRaw) std::memcpy(out, p.raw.data(), kSysexSize);
    else {
        std::memset(out, 0, kSysexSize);
        // fresh image: neutral modifier slots
        for (int i = 0; i < kNumModSlots; ++i) packModifier(out + 1 + kFirstModOffset + 4 * i, Modifier{});
    }
    out[0] = 0xF0;
    out[kSysexSize - 1] = 0xF7;
    uint8_t* d = out + 1;
    std::memcpy(d, kHeader, sizeof(kHeader));
    if (p.hasRaw) d[3] = p.raw[4];                 // keep original device id byte
    const int num = asEditBuffer ? 0 : std::clamp(p.number, 0, 200);
    d[6] = (uint8_t)(num & 0x7F);
    d[7] = (uint8_t)(num >> 7);

    for (int i = 0; i < 20; ++i) {
        char c = i < (int)p.name.size() ? p.name[(size_t)i] : ' ';
        if (c < 32 || c > 126) c = ' ';
        d[8 + i] = (uint8_t)c;
    }
    for (int i = 0; i < kNumModSlots; ++i) packModifier(d + kFirstModOffset + 4 * i, p.mods[(size_t)i]);

    uint32_t bf = readBits28(d + 156);
    bf = setBits(bf, 0, 2, (uint32_t)p.get(pRouting));
    bf = setBits(bf, 2, 7, (uint32_t)(std::clamp(p.get(pOutLevel), -100, 0) + 100));   // hardware range
    bf = setBits(bf, kRelay1Bit, 1, (uint32_t)p.get(pRelay1));
    bf = setBits(bf, kRelay2Bit, 1, (uint32_t)p.get(pRelay2));
    writeBits28(d + 156, bf);

    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef& def = paramDef(i);
        if (def.offset < 0 || def.enc == Enc::Special || def.enc == Enc::None) continue;
        if (algoAware(def.block) && i != algoParamFor(def.block) && !paramUsedBy(def, p.algo(def.block))) continue;
        int val = p.get(i);
        if (isExtendedLevel(i)) { int lo, hi; hardwareRange(i, lo, hi); val = std::clamp(val, lo, hi); }
        if (def.enc == Enc::BoolInv) val = val ? 0 : 1;
        writePacked24(d + def.offset, val);
    }
    d[612] = computeChecksum(d);
}

// ------------------------------------------------------------------ extension message
namespace {
const uint8_t kExtHeader[6] = { 0xF0, 0x7D, 'G', 'M', 'R', 0x01 };   // 0x7D = non-commercial ID
const int kExtParams[] = { pOutLevel, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut };
}

std::vector<uint8_t> encodeExtension(const Patch& p) {
    bool needed = false;
    for (int prm : kExtParams) { int lo, hi; hardwareRange(prm, lo, hi); needed |= p.get(prm) > hi || p.get(prm) < lo; }
    if (!needed) return {};
    std::vector<uint8_t> m(kExtHeader, kExtHeader + 6);
    const int n = (int)(sizeof(kExtParams) / sizeof(kExtParams[0]));
    m.push_back((uint8_t)n);
    for (int prm : kExtParams) {
        const int off = paramDef(prm).offset;
        m.push_back((uint8_t)(off & 0x7F)); m.push_back((uint8_t)(off >> 7));
        uint8_t v[4]; writePacked24(v, p.get(prm)); m.insert(m.end(), v, v + 4);
    }
    m.push_back(0xF7);
    return m;
}

size_t extensionLengthAt(const uint8_t* d, size_t len, size_t pos) noexcept {
    if (pos + 8 > len || std::memcmp(d + pos, kExtHeader, 6) != 0) return 0;
    const size_t total = 6 + 1 + (size_t)d[pos + 6] * 6 + 1;
    if (pos + total > len || d[pos + total - 1] != 0xF7) return 0;
    return total;
}

bool applyExtension(const uint8_t* d, size_t len, Patch& p) noexcept {
    const size_t total = extensionLengthAt(d, len, 0);
    if (total == 0) return false;
    for (int i = 0; i < d[6]; ++i) {
        const uint8_t* e = d + 7 + 6 * i;
        const int off = e[0] | (e[1] << 7);
        for (int prm : kExtParams)
            if (paramDef(prm).offset == off) p.set(prm, readPacked24(e + 2));
    }
    return true;
}

std::vector<uint8_t> encodeForFile(const Patch& p) {
    std::vector<uint8_t> out(kSysexSize);
    encodeSysex(p, out.data());
    const auto ext = encodeExtension(p);
    out.insert(out.end(), ext.begin(), ext.end());
    return out;
}

bool readPresetAt(const uint8_t* data, size_t len, size_t& pos, Patch& p, DecodeResult* result) {
    pos = findNextPreset(data, len, pos);
    if (pos >= len) return false;
    const DecodeResult r = decodeSysex(data + pos, kSysexSize, p);
    if (result) *result = r;
    pos += kSysexSize;
    if (const size_t e = extensionLengthAt(data, len, pos)) {
        if (r.ok) applyExtension(data + pos, e, p);
        pos += e;
    }
    return r.ok;
}

std::array<uint8_t, 10> makePresetRequest(int n) noexcept {
    n = std::clamp(n, 0, 200);
    return { 0xF0, 0x00, 0x20, 0x1F, 0x00, 0x48, 0x45, (uint8_t)(n & 0x7F), (uint8_t)(n >> 7), 0xF7 };
}

size_t findNextPreset(const uint8_t* data, size_t len, size_t start) noexcept {
    for (size_t i = start; i + kSysexSize <= len; ++i) {
        if (data[i] == 0xF0 && data[i + 1] == 0x00 && data[i + 2] == 0x20 && data[i + 3] == 0x1F
            && data[i + 5] == 0x48 && data[i + 6] == 0x20 && data[i + kSysexSize - 1] == 0xF7)
            return i;
    }
    return len;
}

} // namespace gmr
