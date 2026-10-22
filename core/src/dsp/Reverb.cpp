// REVERB block: Spring, Hall, Room, Plate (manual pages 42-45).
// Reconstruction: early-reflection tap pattern (Size + Shape) + 4 input diffusers +
// 8-line feedback delay network with Hadamard mixing, per-line RT60 gains (Decay),
// in-loop high damping (Hi Color/Factor), output low shelf (Lo Color/Factor).
// "Room Level" = tail level, "Reverb Lev" = early reflections level (manual wording).
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

namespace {
const float kBaseMs[ReverbBlock::kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 61.3f, 67.1f, 79.3f };
const float kSizeScale[8] = { 0.30f, 0.40f, 0.55f, 0.70f, 0.85f, 1.0f, 1.2f, 1.4f };          // Box .. Huge
const float kHiColorHz[7] = { 1500.0f, 3000.0f, 5000.0f, 7000.0f, 10000.0f, 13000.0f, 16000.0f }; // Wool .. Grand
const float kLoColorDb[7] = { 4.0f, 2.0f, 0.0f, -2.0f, -4.5f, -8.0f, -14.0f };                   // Thick .. No Base
const float kDiffMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
const float kSpringMs[6] = { 1.1f, 1.7f, 2.3f, 2.9f, 3.7f, 4.3f };

void hadamard8(float* x) noexcept {
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) { const float a = x[j], b = x[j + h]; x[j] = a + b; x[j + h] = a - b; }
    for (int i = 0; i < 8; ++i) x[i] *= 0.35355339f;   // 1/sqrt(8): orthonormal
}
} // namespace

void ReverbBlock::prepare(float sr, int) {
    fs = sr;
    pre.allocate((int)(sr * 0.11f) + 8);
    for (auto& e : er) e.allocate((int)(sr * 0.16f) + 8);
    for (auto& d : line) d.allocate((int)(sr * 0.13f) + 8);
    for (auto& a : diffL) a.d.allocate((int)(sr * 0.02f) + 8);
    for (auto& a : diffR) a.d.allocate((int)(sr * 0.02f) + 8);
    for (auto& a : spring) a.d.allocate((int)(sr * 0.01f) + 8);
    configured = false;
    reset();
}

void ReverbBlock::reset() {
    pre.reset();
    for (auto& e : er) e.reset();
    for (auto& d : line) d.reset();
    for (auto& d : damp) d.reset();
    for (auto& b : lowShelf) b.reset();
    for (auto& a : diffL) a.d.reset();
    for (auto& a : diffR) a.d.reset();
    for (auto& a : spring) a.d.reset();
    lfo.phase = 0.0;
}

void ReverbBlock::configure(const Ctx& c) {
    const int a = c.i(pRevAlgo);
    float algoScale = 1.0f, erScale = 1.0f;
    switch (a) {
        case algo::Spring: algoScale = 0.45f; erScale = 0.3f; springK = 0.68f; diffK = 0.55f; break;
        case algo::Hall:   algoScale = 1.0f;  erScale = 1.0f; springK = 0.0f;  diffK = 0.70f; break;
        case algo::Room:   algoScale = 0.55f; erScale = 1.3f; springK = 0.0f;  diffK = 0.62f; break;
        default:           algoScale = 0.75f; erScale = 0.15f; springK = 0.0f; diffK = 0.78f; break;  // Plate
    }
    diffK = clampf(diffK + c.val(pRevDiffuse) / 25.0f * 0.12f, 0.3f, 0.85f);
    const float scale = algoScale * kSizeScale[std::clamp(c.i(pRevSize), 0, 7)];
    const float rt60 = std::max(0.1f, c.val(pRevDecay) * 0.1f);
    const float hiHz = kHiColorHz[std::clamp(c.i(pRevHiColor), 0, 6)] * std::exp2(c.val(pRevHiFactor) / 25.0f);
    const float loDb = kLoColorDb[std::clamp(c.i(pRevLoColor), 0, 6)] + c.val(pRevLoFactor) / 25.0f * 6.0f;

    for (int i = 0; i < kLines; ++i) {
        len[(size_t)i] = kBaseMs[i] * scale * 0.001f * fs;
        g[(size_t)i] = std::pow(10.0f, -3.0f * len[(size_t)i] / (rt60 * fs));
        damp[(size_t)i].setCutoff(std::min(hiHz, 0.44f * fs), fs);
    }
    lowShelf[0].c = BiquadCoefs::lowShelf(250.0f, loDb, fs);
    lowShelf[1].c = lowShelf[0].c;
    for (int i = 0; i < 4; ++i) {
        diffL[(size_t)i].len = kDiffMs[i] * 0.001f * fs * std::max(0.5f, scale);
        diffR[(size_t)i].len = kDiffMs[3 - i] * 0.001f * fs * std::max(0.5f, scale) * 1.07f;
    }
    for (int i = 0; i < 6; ++i) spring[(size_t)i].len = kSpringMs[i] * 0.001f * fs;

    // Early reflections: 12 taps spread over 5..80 ms x size, gain envelope by Shape.
    const int shape = std::clamp(c.i(pRevShape), 0, 2);
    for (int t = 0; t < 12; ++t) {
        const float x = (float)t / 11.0f;
        erTapMs[(size_t)t] = (5.0f + 75.0f * x * (0.85f + 0.3f * (float)((t * 7) % 5) / 4.0f)) * scale;
        float env;
        if (shape == 0) env = std::exp(-3.0f * x);                        // Round: smooth decay
        else if (shape == 1) env = std::sin(kPi * std::min(1.0f, x * 1.3f + 0.1f)); // Curved: build-up then fall
        else env = x < 0.8f ? 1.0f : 0.25f;                              // Square: flat, abrupt end
        erTapGain[(size_t)t] = env * erScale * 0.28f * ((t & 1) ? -1.0f : 1.0f);
    }
    configured = true;
}

void ReverbBlock::process(float* l, float* r, int n, const Ctx& c) {
    const float now[10] = { c.raw(pRevAlgo), c.raw(pRevDecay), c.raw(pRevShape), c.raw(pRevSize), c.raw(pRevHiColor),
                            c.raw(pRevHiFactor), c.raw(pRevLoColor), c.raw(pRevLoFactor), c.raw(pRevDiffuse), c.sr };
    bool changed = !configured;
    for (int i = 0; i < 10; ++i) if (now[i] != cfg[(size_t)i]) { changed = true; cfg[(size_t)i] = now[i]; }
    if (changed) configure(c);

    const float preS = 1.0f + c.val(pRevPreDelay) * 0.001f * fs;
    const float tailGain = dbToGain(c.val(pRevRoomLevel)) * 0.6f;
    const float erGain = dbToGain(c.val(pRevLevel));
    const bool modulate = c.i(pRevAlgo) == algo::Hall || c.i(pRevAlgo) == algo::Plate;
    const float modDepth = 0.0004f * fs;
    float erS[12];
    for (int t = 0; t < 12; ++t) erS[t] = 1.0f + erTapMs[(size_t)t] * 0.001f * fs;

    for (int k = 0; k < n; ++k) {
        const float x = 0.5f * (l[k] + r[k]);
        pre.push(x);
        const float xp = pre.read(preS);

        er[0].push(xp);
        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < 12; t += 2) erL += er[0].read(erS[t]) * erTapGain[(size_t)t];
        for (int t = 1; t < 12; t += 2) erR += er[0].read(erS[t]) * -erTapGain[(size_t)t];

        float dIn = xp;
        if (springK > 0.0f) for (auto& a : spring) dIn = a.process(dIn, springK);
        float dL = dIn, dR = dIn;
        for (auto& a : diffL) dL = a.process(dL, diffK);
        for (auto& a : diffR) dR = a.process(dR, diffK);

        const float ph = lfo.advance(0.7f, fs);
        float o[kLines];
        for (int i = 0; i < kLines; ++i) {
            float d = len[(size_t)i];
            if (modulate && (i == 1 || i == 6)) d += modDepth * Lfo::sine(ph + 0.37f * (float)i);
            o[i] = line[(size_t)i].read(d);
        }
        float tl = 0.0f, tr = 0.0f;
        for (int i = 0; i < kLines; i += 2) { tl += o[i]; tr += o[i + 1]; }
        float fbv[kLines];
        for (int i = 0; i < kLines; ++i) fbv[i] = damp[(size_t)i].process(o[i]) * g[(size_t)i];
        hadamard8(fbv);
        for (int i = 0; i < kLines; ++i) {
            const float inj = (i & 1) ? dR : dL;
            line[(size_t)i].push(flushDenormal(fbv[i] + inj * ((i & 2) ? -0.5f : 0.5f)));
        }
        tl = lowShelf[0].process(tl);
        tr = lowShelf[1].process(tr);
        l[k] = tl * tailGain + erL * erGain;
        r[k] = tr * tailGain + erR * erGain;
    }
}

} // namespace gmr::dsp
