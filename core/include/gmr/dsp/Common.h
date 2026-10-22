// G-Major Reborn — DSP building blocks. Header-only, allocation-free after prepare().
#pragma once
#include "gmr/Params.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gmr::dsp {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

/** -100 dB is the G-Major "Off" position of every level control. */
inline float dbToGain(float db) noexcept { return db <= -99.5f ? 0.0f : std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g) noexcept { return g <= 1e-9f ? -180.0f : 20.0f * std::log10(g); }
inline float flushDenormal(float x) noexcept { return std::fabs(x) < 1e-20f ? 0.0f : x; }
inline float clampf(float x, float lo, float hi) noexcept { return std::min(hi, std::max(lo, x)); }

/** Context handed to every block for one sub-block. `s` = effective stored-domain values (float, modifiers applied). */
struct Ctx {
    float sr = 48000.0f;
    float beatMs = 500.0f;   // quarter note duration (tap tempo or host)
    const float* s = nullptr;
    bool killDry = false;

    float raw(int p) const noexcept { return s[p]; }
    int i(int p) const noexcept { return (int)std::lround(s[p]); }
    /** Natural-unit value, interpolating table entries for fractional (modulated) positions. */
    float val(int p) const noexcept {
        const ParamDef& d = paramDef(p);
        const float x = clampf(s[p], (float)d.lo, (float)d.hi);
        if (d.kind != Kind::Table) return x;
        const int i0 = (int)std::floor(x);
        const float fr = x - (float)i0;
        const float a = tableValue(*d.table, i0);
        if (fr <= 1e-4f) return a;
        const float b = tableValue(*d.table, i0 + 1);
        if (a < 0.0f || b < 0.0f) return tableValue(*d.table, (int)std::lround(x));
        return a + (b - a) * fr;
    }
    /** Duration in ms of a tempo-table entry (0 = Ignored -> returns -1). Index 1 = "1" = one bar (4 beats). */
    float tempoMs(int tempoParam) const noexcept {
        const int idx = i(tempoParam);
        if (idx <= 0) return -1.0f;
        return beatMs * 4.0f * tableValue(tables::tapTempo, idx);
    }
    /** LFO rate: Speed parameter unless a Tempo subdivision is selected (manual p.30). */
    float rateHz(int speedParam, int tempoParam) const noexcept {
        const float ms = tempoMs(tempoParam);
        return ms > 0.0f ? 1000.0f / ms : val(speedParam);
    }
};

// ----------------------------------------------------------------------- smoothing
struct Smoother {
    float cur = 0.0f, target = 0.0f, coef = 0.0f;
    void setTime(float ms, float sr) noexcept { coef = std::exp(-1.0f / std::max(1.0f, ms * 0.001f * sr)); }
    void reset(float v) noexcept { cur = target = v; }
    float next() noexcept { cur = target + (cur - target) * coef; return cur; }
};

// ----------------------------------------------------------------------- biquad (TDF-II)
struct BiquadCoefs {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoefs lowpass(float f, float q, float sr) noexcept { return rbj(0, f, q, 0, sr); }
    static BiquadCoefs highpass(float f, float q, float sr) noexcept { return rbj(1, f, q, 0, sr); }
    static BiquadCoefs peaking(float f, float bwOct, float gainDb, float sr) noexcept {
        // RBJ cookbook, bandwidth in octaves
        f = clampf(f, 10.0f, 0.49f * sr);
        const float w0 = kTwoPi * f / sr, sw = std::sin(w0), cw = std::cos(w0);
        const float alpha = sw * std::sinh(std::log(2.0f) * 0.5f * bwOct * w0 / std::max(1e-6f, sw));
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float a0 = 1.0f + alpha / A;
        return { (1.0f + alpha * A) / a0, (-2.0f * cw) / a0, (1.0f - alpha * A) / a0, (-2.0f * cw) / a0, (1.0f - alpha / A) / a0 };
    }
    static BiquadCoefs lowShelf(float f, float gainDb, float sr) noexcept { return shelf(false, f, gainDb, sr); }
    static BiquadCoefs highShelf(float f, float gainDb, float sr) noexcept { return shelf(true, f, gainDb, sr); }

private:
    static BiquadCoefs rbj(int type, float f, float q, float, float sr) noexcept {
        f = clampf(f, 10.0f, 0.49f * sr);
        q = std::max(0.1f, q);
        const float w0 = kTwoPi * f / sr, cw = std::cos(w0), alpha = std::sin(w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        if (type == 0) return { (1 - cw) * 0.5f / a0, (1 - cw) / a0, (1 - cw) * 0.5f / a0, -2 * cw / a0, (1 - alpha) / a0 };
        return { (1 + cw) * 0.5f / a0, -(1 + cw) / a0, (1 + cw) * 0.5f / a0, -2 * cw / a0, (1 - alpha) / a0 };
    }
    static BiquadCoefs shelf(bool high, float f, float gainDb, float sr) noexcept {
        f = clampf(f, 10.0f, 0.49f * sr);
        const float A = std::pow(10.0f, gainDb / 40.0f), w0 = kTwoPi * f / sr;
        const float cw = std::cos(w0), sw = std::sin(w0), alpha = sw * 0.5f * std::sqrt(2.0f);
        const float sA = 2.0f * std::sqrt(A) * alpha;
        float b0, b1, b2, a0, a1, a2;
        if (!high) {
            b0 = A * ((A + 1) - (A - 1) * cw + sA); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sA);
            a0 = (A + 1) + (A - 1) * cw + sA;       a1 = -2 * ((A - 1) + (A + 1) * cw);    a2 = (A + 1) + (A - 1) * cw - sA;
        } else {
            b0 = A * ((A + 1) + (A - 1) * cw + sA); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sA);
            a0 = (A + 1) - (A - 1) * cw + sA;       a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - sA;
        }
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }
};

struct Biquad {
    BiquadCoefs c;
    float z1 = 0, z2 = 0;
    void reset() noexcept { z1 = z2 = 0; }
    float process(float x) noexcept {
        const float y = c.b0 * x + z1;
        z1 = flushDenormal(c.b1 * x - c.a1 * y + z2);
        z2 = flushDenormal(c.b2 * x - c.a2 * y);
        return y;
    }
};

// ----------------------------------------------------------------------- one-pole filters
struct OnePoleLP {
    float a = 0, z = 0;
    void setCutoff(float f, float sr) noexcept {
        if (f <= 0.0f || f >= 0.45f * sr) { a = 0.0f; return; }   // Off / bypass
        a = std::exp(-kTwoPi * f / sr);
    }
    void reset() noexcept { z = 0; }
    float process(float x) noexcept { z = flushDenormal(x + a * (z - x)); return z; }
};
struct OnePoleHP {
    OnePoleLP lp; bool on = false;
    void setCutoff(float f, float sr) noexcept { on = f > 0.0f; if (on) lp.setCutoff(f, sr); }
    void reset() noexcept { lp.reset(); }
    float process(float x) noexcept { return on ? x - lp.process(x) : x; }
};

/** Chamberlin/Simper state-variable filter (TPT form) — stable under fast modulation. */
struct Svf {
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set(float f, float q, float sr) noexcept {
        f = clampf(f, 10.0f, 0.47f * sr);
        g = std::tan(kPi * f / sr);
        k = 1.0f / std::max(0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0; }
    /** returns lowpass, writes band/high if requested */
    float lp(float x) noexcept {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal(2 * v1 - ic1); ic2 = flushDenormal(2 * v2 - ic2);
        return v2;
    }
};

// ----------------------------------------------------------------------- delay line
class DelayLine {
public:
    void allocate(int maxSamples) {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign((size_t)n, 0.0f);
        mask = n - 1; w = 0;
    }
    void reset() noexcept { std::fill(buf.begin(), buf.end(), 0.0f); w = 0; }
    void push(float x) noexcept { buf[(size_t)w] = x; w = (w + 1) & mask; }
    /** Delay in samples (>= 1 for a read after push), cubic Hermite interpolation. */
    float read(float d) const noexcept {
        d = clampf(d, 1.0f, (float)(mask - 3));
        const float rp = (float)w - d;
        const int i = (int)std::floor(rp);
        const float f = rp - (float)i;
        const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    float readInt(int d) const noexcept { return at(w - std::max(1, std::min(d, mask))); }
    int capacity() const noexcept { return mask; }
private:
    float at(int i) const noexcept { return buf[(size_t)(i & mask)]; }
    std::vector<float> buf { 0.0f, 0.0f, 0.0f, 0.0f };
    int mask = 3, w = 0;
};

// ----------------------------------------------------------------------- LFO
struct Lfo {
    double phase = 0.0;  // 0..1
    float advance(float hz, float sr) noexcept {
        phase += (double)hz / (double)sr;
        phase -= std::floor(phase);
        return (float)phase;
    }
    static float sine(float ph) noexcept { return std::sin(kTwoPi * ph); }
    static float tri(float ph) noexcept { return 1.0f - 4.0f * std::fabs(ph - std::floor(ph + 0.5f) ); }
};

// ----------------------------------------------------------------------- envelope follower
struct EnvFollower {
    float env = 0, att = 0, rel = 0;
    void set(float attackMs, float releaseMs, float sr) noexcept {
        att = std::exp(-1.0f / std::max(1.0f, attackMs * 0.001f * sr));
        rel = std::exp(-1.0f / std::max(1.0f, releaseMs * 0.001f * sr));
    }
    void reset() noexcept { env = 0; }
    float process(float x) noexcept {
        const float a = std::fabs(x);
        env = flushDenormal(a > env ? a + att * (env - a) : a + rel * (env - a));
        return env;
    }
};

/** Equal-power pan, p in [-1, 1]. */
inline void panGains(float p, float& gl, float& gr) noexcept {
    const float a = (clampf(p, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    gl = std::cos(a) * 1.41421356f * 0.70710678f;  // unity at extremes, -3 dB centre
    gr = std::sin(a) * 1.41421356f * 0.70710678f;
}

/** Base class of the eight effect blocks. process() turns the buffer into the 100 % wet signal. */
class FxBlock {
public:
    virtual ~FxBlock() = default;
    virtual void prepare(float sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process(float* l, float* r, int n, const Ctx& c) = 0;
    /** Time (ms) the block keeps ringing after input stops — used to keep tails alive. */
    virtual float tailMs() const { return 0.0f; }
};

} // namespace gmr::dsp
