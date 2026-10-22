// FILTER/MOD block: Auto Resonance, Resonance Filter, Vintage/Smooth Phaser, Tremolo, Panner.
// Manual pages 34-36.
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

void FilterMod::prepare(float sr, int) {
    fs = sr;
    cutoff.setTime(1.0f, sr);
    lfoGain.setTime(1.5f, sr);
    reset();
}

void FilterMod::reset() {
    for (auto& s : svf) s.reset();
    env.reset();
    cutoff.reset(1000.0f);
    lfo.phase = 0.0;
    for (auto& ch : ap) for (auto& a : ch) a.z = 0.0f;
    phFb = { 0.0f, 0.0f };
    for (auto& h : hicut) h.reset();
    lfoGain.reset(1.0f);
}

void FilterMod::process(float* l, float* r, int n, const Ctx& c) {
    switch (c.i(pFltAlgo)) {
        case algo::AutoRes:       autoRes(l, r, n, c); break;
        case algo::Resonance:     resonance(l, r, n, c); break;
        case algo::VintagePhaser: phaser(l, r, n, c, 4); break;    // manual: four all-pass filters
        case algo::SmoothPhaser:  phaser(l, r, n, c, 12); break;   // manual: twelve all-pass filters
        case algo::Tremolo:       tremolo(l, r, n, c); break;
        case algo::Panner:        panner(l, r, n, c); break;
        default: break;
    }
}

// Touch-wah: input envelope sweeps a resonant low-pass from 200 Hz up to Freq Max.
void FilterMod::autoRes(float* l, float* r, int n, const Ctx& c) {
    static const float attMs[3] = { 25.0f, 8.0f, 2.0f };     // Slow / Medium / Fast (inferred)
    static const float relMs[3] = { 450.0f, 200.0f, 70.0f };
    const int resp = std::clamp(c.i(pFltResponse), 0, 2);
    env.set(attMs[resp], relMs[resp], fs);
    const float sens = std::pow(2.0f, c.val(pFltSens)) * 0.08f;   // 0..10 -> x0.08 .. x82
    const float fMin = 200.0f, fMax = std::max(fMin * 1.5f, c.val(pFltFreqMax));
    const bool fourth = c.i(pFltOrder) == 1;
    const float q = fourth ? 2.2f : 4.5f;
    for (int k = 0; k < n; ++k) {
        const float e = clampf(env.process(0.5f * (l[k] + r[k])) * sens, 0.0f, 1.0f);
        cutoff.target = fMin * std::pow(fMax / fMin, e);
        const float fc = cutoff.next();
        if ((k & 3) == 0) {
            for (int ch = 0; ch < 2; ++ch) { svf[(size_t)ch * 2].set(fc, q, fs); svf[(size_t)ch * 2 + 1].set(fc, q, fs); }
        }
        float yl = svf[0].lp(l[k]), yr = svf[2].lp(r[k]);
        if (fourth) { yl = svf[1].lp(yl); yr = svf[3].lp(yr); }
        l[k] = yl; r[k] = yr;
    }
}

// Static resonant low-pass: Hi Cut + Hi Resonance, 12 or 24 dB/oct.
void FilterMod::resonance(float* l, float* r, int n, const Ctx& c) {
    const float fc = c.val(pFltResHiCut);
    const float res = c.val(pFltResonance) * 0.01f;
    const bool fourth = c.i(pFltOrder) == 1;
    const float q = 0.707f + res * res * 11.3f;
    for (int ch = 0; ch < 2; ++ch) {
        svf[(size_t)ch * 2].set(fc, fourth ? 0.54f : q, fs);
        svf[(size_t)ch * 2 + 1].set(fc, q, fs);
    }
    for (int k = 0; k < n; ++k) {
        float yl = svf[0].lp(l[k]), yr = svf[2].lp(r[k]);
        if (fourth) { yl = svf[1].lp(yl); yr = svf[3].lp(yr); }
        l[k] = yl; r[k] = yr;
    }
}

// All-pass chain phaser. Range Low/High chooses the sweep band; Phase Reverse offsets the
// right channel LFO by 180 degrees ("starts the waveform at two different points").
void FilterMod::phaser(float* l, float* r, int n, const Ctx& c, int stages) {
    const float rate = c.rateHz(pFltSpeed, pFltTempo);
    const float depth = c.val(pFltDepth) * 0.01f;
    const float fb = clampf(c.val(pFltFeedback) * 0.01f, -1.0f, 1.0f) * 0.85f;
    const bool high = c.i(pFltPhRange) == 1;
    const float fLo = high ? 400.0f : 100.0f, fHi = high ? 6400.0f : 1600.0f;
    const float ratio = fHi / fLo;
    const bool rev = c.i(pFltPhReverse) == 1;
    float* io[2] = { l, r };
    for (int k = 0; k < n; ++k) {
        const float ph = lfo.advance(rate, fs);
        for (int ch = 0; ch < 2; ++ch) {
            const float p = ch == 1 && rev ? ph + 0.5f : ph;
            const float m = 0.5f + 0.5f * depth * Lfo::sine(p);
            const float fc = fLo * std::pow(ratio, m);
            const float t = std::tan(kPi * std::min(fc, 0.45f * fs) / fs);
            const float a = (t - 1.0f) / (t + 1.0f);
            float x = io[ch][k] + fb * phFb[(size_t)ch];
            for (int s = 0; s < stages; ++s) x = ap[(size_t)ch][(size_t)s].process(x, a);
            phFb[(size_t)ch] = flushDenormal(x);
            io[ch][k] = x;
        }
    }
}

// Tremolo: Soft = raised-cosine, Hard = square; Pulse Width splits the cycle; Hi Cut on output.
void FilterMod::tremolo(float* l, float* r, int n, const Ctx& c) {
    const float rate = c.rateHz(pFltSpeed, pFltTempo);
    const float depth = c.val(pFltDepth) * 0.01f;
    const float pw = clampf(c.val(pFltPulseWidth) * 0.01f, 0.01f, 0.99f);
    const bool hard = c.i(pFltTremType) == 1;
    const float hc = c.val(pFltTremHiCut);
    hicut[0].setCutoff(hc, fs); hicut[1].setCutoff(hc, fs);
    for (int k = 0; k < n; ++k) {
        const float ph = lfo.advance(rate, fs);
        float u;  // 1 = full level, 0 = maximum cut
        if (hard) u = ph < pw ? 1.0f : 0.0f;
        else {
            const float q = ph < pw ? 0.5f * ph / pw : 0.5f + 0.5f * (ph - pw) / (1.0f - pw);
            u = 0.5f + 0.5f * std::cos(kTwoPi * q);
        }
        lfoGain.target = 1.0f - depth * (1.0f - u);
        const float g = lfoGain.next();             // de-click hard edges
        l[k] = hicut[0].process(l[k] * g);
        r[k] = hicut[1].process(r[k] * g);
    }
}

// Auto-panner: sine LFO, Width = sweep amount (stored in the Depth field, offset 248).
void FilterMod::panner(float* l, float* r, int n, const Ctx& c) {
    const float rate = c.rateHz(pFltSpeed, pFltTempo);
    const float width = c.val(pFltDepth) * 0.01f;
    for (int k = 0; k < n; ++k) {
        const float ph = lfo.advance(rate, fs);
        const float p = width * Lfo::sine(ph);
        const float a = (p + 1.0f) * 0.25f * kPi;
        const float m = 0.5f * (l[k] + r[k]);
        l[k] = m * std::cos(a) * 1.41421356f;
        r[k] = m * std::sin(a) * 1.41421356f;
    }
}

} // namespace gmr::dsp
