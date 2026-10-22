// Noise gate, 3-band EQ and compressor.
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

// ---------------------------------------------------------------------------- Noise gate
// Manual p.27-28: Threshold -60..0 dB, Max Damping 0..90 dB, Release 3..200 dB/s
// (speed at which the signal is RE-OPENED once input rises above threshold),
// Mode Hard/Soft = how fast attenuation is applied below threshold.
// Model (inferred): dB-domain gain with constant-rate slews.
//   Hard: closes at 1000 dB/s to -MaxDamp.
//   Soft: downward expander 1:3 below threshold (dotted line in the manual plot),
//         limited to -MaxDamp, closing at 120 dB/s.
void NoiseGate::process(float* l, float* r, int n, const Ctx& c) {
    const float thr = c.val(pGateThreshold);
    const float maxDamp = c.val(pGateMaxDamp);
    const float openRate = c.val(pGateRelease) / fs;            // dB per sample
    const bool hard = c.i(pGateMode) == 1;
    const float closeRate = (hard ? 1000.0f : 120.0f) / fs;
    const int holdSamples = (int)(0.015f * fs);

    for (int k = 0; k < n; ++k) {
        const float lev = gainToDb(det.process(std::max(std::fabs(l[k]), std::fabs(r[k]))));
        float target;
        if (lev >= thr) { open = true; holdLeft = holdSamples; }
        else if (lev < thr - 3.0f) {
            if (holdLeft > 0) --holdLeft; else open = false;
        }
        if (open) target = 0.0f;
        else target = hard ? -maxDamp : std::max(-maxDamp, (lev - thr) * 2.0f);   // 1:3 expansion => gain = 2*(lev-thr)

        if (gainDb < target) gainDb = std::min(target, gainDb + openRate);
        else gainDb = std::max(target, gainDb - closeRate);
        const float g = dbToGain(gainDb);
        l[k] *= g; r[k] *= g;
    }
}

// ---------------------------------------------------------------------------- EQ
// Manual p.28: three peaking bands, Freq 40 Hz..20 kHz (or Off), Gain ±12 dB, Width 0.2..4 oct.
void Equalizer::process(float* l, float* r, int n, const Ctx& c) {
    const int fp[3] = { pEq1Freq, pEq2Freq, pEq3Freq };
    const int gp[3] = { pEq1Gain, pEq2Gain, pEq3Gain };
    const int wp[3] = { pEq1Width, pEq2Width, pEq3Width };
    for (int b = 0; b < 3; ++b) {
        const float f = c.val(fp[b]), gdb = c.val(gp[b]), w = c.val(wp[b]);
        if (dirty || f != last[(size_t)b * 3] || gdb != last[(size_t)b * 3 + 1] || w != last[(size_t)b * 3 + 2]) {
            last[(size_t)b * 3] = f; last[(size_t)b * 3 + 1] = gdb; last[(size_t)b * 3 + 2] = w;
            active[(size_t)b] = f > 0.0f && std::fabs(gdb) > 0.01f;
            if (active[(size_t)b]) {
                const auto co = BiquadCoefs::peaking(f, w, gdb, fs);
                bands[(size_t)b][0].c = co; bands[(size_t)b][1].c = co;
            }
        }
    }
    dirty = false;
    for (int b = 0; b < 3; ++b) {
        if (!active[(size_t)b]) continue;
        auto& fl = bands[(size_t)b][0]; auto& fr = bands[(size_t)b][1];
        for (int k = 0; k < n; ++k) { l[k] = fl.process(l[k]); r[k] = fr.process(r[k]); }
    }
}

// ---------------------------------------------------------------------------- Compressor
// Manual p.29: Threshold, Ratio Off..Inf:1, Attack, Release, Gain ±6 dB.
// Model: feed-forward, stereo-linked peak detector, 4 dB soft knee, dB-domain ballistics.
void Compressor::process(float* l, float* r, int n, const Ctx& c) {
    const float thr = c.val(pCompThreshold);
    const float ratio = c.val(pCompRatio);
    const float slope = ratio >= 1e8f ? 1.0f : (1.0f - 1.0f / std::max(1.0f, ratio));
    const float att = std::exp(-1.0f / std::max(1.0f, c.val(pCompAttack) * 0.001f * fs));
    const float rel = std::exp(-1.0f / std::max(1.0f, c.val(pCompRelease) * 0.001f * fs));
    const float makeup = dbToGain(c.val(pCompGain));
    constexpr float knee = 4.0f;

    for (int k = 0; k < n; ++k) {
        const float x = gainToDb(std::max(std::fabs(l[k]), std::fabs(r[k])));
        const float over = x - thr;
        float want = 0.0f;
        if (slope > 0.0f) {
            if (over >= knee * 0.5f) want = -slope * over;
            else if (over > -knee * 0.5f) { const float t = over + knee * 0.5f; want = -slope * t * t / (2.0f * knee); }
        }
        // attack when more reduction is needed, release otherwise
        grDb = want < grDb ? want + att * (grDb - want) : want + rel * (grDb - want);
        const float g = dbToGain(grDb) * makeup;
        l[k] *= g; r[k] *= g;
    }
}

} // namespace gmr::dsp
