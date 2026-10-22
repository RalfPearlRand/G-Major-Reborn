// CHORUS/FLANGE block: Classic/Advanced Chorus, Classic/Advanced Flanger, Vibrato (manual 30-33).
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

void ChorusFlanger::prepare(float sr, int) {
    fs = sr;
    for (auto& d : dl) d.allocate((int)(sr * 0.065f) + 8);   // 50 ms base + 12 ms excursion
    baseDelay.setTime(40.0f, sr);
    excursion.setTime(40.0f, sr);
    reset();
}

void ChorusFlanger::reset() {
    for (auto& d : dl) d.reset();
    for (auto& h : hicut) h.reset();
    for (auto& h : fbCut) h.reset();
    fb = { 0.0f, 0.0f };
    lfo.phase = 0.0;
    baseDelay.reset(-1.0f);
}

void ChorusFlanger::process(float* l, float* r, int n, const Ctx& c) {
    const int a = c.i(pModAlgo);
    const float rate = c.rateHz(pModSpeed, pModTempo);
    const float depth = c.val(pModDepth) * 0.01f;
    const bool flange = a == algo::ClassicFlange || a == algo::AdvFlange;
    const bool advanced = a == algo::AdvChorus || a == algo::AdvFlange;

    // Base delay and maximum excursion (ms). Advanced algorithms expose the delay (0.1-50 ms).
    float baseMs, excMs;
    switch (a) {
        case algo::ClassicChorus: baseMs = 7.0f;  excMs = 6.0f; break;
        case algo::AdvChorus:     baseMs = c.val(pModDelay) * 0.1f; excMs = 6.0f; break;
        case algo::ClassicFlange: baseMs = 0.4f;  excMs = 4.5f; break;
        case algo::AdvFlange:     baseMs = c.val(pModDelay) * 0.1f; excMs = 4.5f; break;
        default:                  baseMs = 0.5f;  excMs = 5.0f; break;   // Vibrato
    }
    excMs *= depth;
    // Golden Ratio (TC 2290): depth is reduced as speed rises so the perceived amount
    // (pitch deviation ~ excursion x rate) stays constant. Classic Chorus always uses it.
    const bool gold = a == algo::ClassicChorus || (advanced && c.i(pModGoldRatio) == 1);
    if (gold) excMs *= clampf(0.6f / std::max(0.05f, rate), 0.04f, 1.0f);

    baseDelay.target = baseMs * 0.001f * fs;
    excursion.target = excMs * 0.001f * fs;
    if (baseDelay.cur < 0.0f) { baseDelay.reset(baseDelay.target); excursion.reset(excursion.target); }

    const float fbAmt = flange ? clampf(c.val(pModFeedback) * 0.01f, -1.0f, 1.0f) * 0.96f : 0.0f;
    const float hc = c.val(pModHiCut), fbhc = c.val(pModFbHiCut);
    for (int ch = 0; ch < 2; ++ch) { hicut[(size_t)ch].setCutoff(hc, fs); fbCut[(size_t)ch].setCutoff(fbhc, fs); }
    const float stereoOffset = (a == algo::ClassicChorus || a == algo::AdvChorus) ? 0.25f : 0.0f;
    const float rightSign = (advanced && c.i(pModPhReverse) == 1) ? -1.0f : 1.0f;

    float* io[2] = { l, r };
    for (int k = 0; k < n; ++k) {
        const float ph = lfo.advance(rate, fs);
        const float bd = baseDelay.next(), ex = excursion.next();
        for (int ch = 0; ch < 2; ++ch) {
            const float p = ph + (ch == 1 ? stereoOffset : 0.0f);
            const float d = 1.0f + bd + ex * (0.5f + 0.5f * Lfo::sine(p));
            auto& line = dl[(size_t)ch];
            const float wet = line.read(d);
            const float in = io[ch][k] + (flange ? fbCut[(size_t)ch].process(fb[(size_t)ch]) * fbAmt : 0.0f);
            // transparent below 0 dBFS; soft safety only against runaway feedback
            line.push(std::fabs(in) < 1.0f ? in : std::copysign(1.0f + std::tanh(std::fabs(in) - 1.0f), in));
            fb[(size_t)ch] = flushDenormal(wet);
            io[ch][k] = hicut[(size_t)ch].process(wet) * (ch == 1 ? rightSign : 1.0f);
        }
    }
}

} // namespace gmr::dsp
