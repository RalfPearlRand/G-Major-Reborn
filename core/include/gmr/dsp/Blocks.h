// G-Major Reborn — the eight effect blocks of the G-Major signal chain.
// Each block reads its parameters from Ctx (stored-domain values) and is
// allocation-free in process(). Algorithms are reconstructions from the
// owner's manual — see docs/DSP_NOTES.md for every modelling assumption.
#pragma once
#include "gmr/dsp/Common.h"
#include <array>

namespace gmr::dsp {

// =========================================================================== Noise gate
class NoiseGate final : public FxBlock {
public:
    void prepare(float sr, int) override { fs = sr; det.set(0.1f, 40.0f, sr); reset(); }
    void reset() override { det.reset(); gainDb = 0.0f; open = true; holdLeft = 0; }
    void process(float* l, float* r, int n, const Ctx& c) override;
    float currentGainDb() const { return gainDb; }
private:
    float fs = 48000.0f, gainDb = 0.0f;
    bool open = true;
    int holdLeft = 0;
    EnvFollower det;
};

// =========================================================================== 3-band parametric EQ
class Equalizer final : public FxBlock {
public:
    void prepare(float sr, int) override { fs = sr; dirty = true; reset(); }
    void reset() override { for (auto& b : bands) { b[0].reset(); b[1].reset(); } }
    void process(float* l, float* r, int n, const Ctx& c) override;
private:
    float fs = 48000.0f;
    bool dirty = true;
    std::array<std::array<Biquad, 2>, 3> bands {};
    std::array<bool, 3> active {};
    std::array<float, 9> last {};
};

// =========================================================================== Compressor
class Compressor final : public FxBlock {
public:
    void prepare(float sr, int) override { fs = sr; reset(); }
    void reset() override { grDb = 0.0f; }
    void process(float* l, float* r, int n, const Ctx& c) override;
    float gainReductionDb() const { return grDb; }
private:
    float fs = 48000.0f, grDb = 0.0f;
};

// =========================================================================== Filter / Mod block
class FilterMod final : public FxBlock {
public:
    void prepare(float sr, int) override;
    void reset() override;
    void process(float* l, float* r, int n, const Ctx& c) override;
private:
    void autoRes(float* l, float* r, int n, const Ctx& c);
    void resonance(float* l, float* r, int n, const Ctx& c);
    void phaser(float* l, float* r, int n, const Ctx& c, int stages);
    void tremolo(float* l, float* r, int n, const Ctx& c);
    void panner(float* l, float* r, int n, const Ctx& c);
    float fs = 48000.0f;
    std::array<Svf, 4> svf {};           // [ch*2 + stage]
    EnvFollower env;
    Smoother cutoff;
    Lfo lfo;
    struct Ap1 { float z = 0; float process(float x, float a) noexcept { const float y = -a * x + z; z = flushDenormal(x + a * y); return y; } };
    std::array<std::array<Ap1, 12>, 2> ap {};
    std::array<float, 2> phFb {};
    std::array<OnePoleLP, 2> hicut {};
    Smoother lfoGain;
};

// =========================================================================== Pitch block
/** Two-head rotating-tap pitch shifter (crossfaded Hann windows). */
class PitchVoice {
public:
    void prepare(float sr, float windowMs);
    void reset();
    void setWindowMs(float ms) noexcept;
    float process(float x, float ratio) noexcept;
private:
    DelayLine dl;
    float fs = 48000.0f, win = 2048.0f, phase = 0.0f;
};

class PitchBlock final : public FxBlock {
public:
    void prepare(float sr, int) override;
    void reset() override;
    void process(float* l, float* r, int n, const Ctx& c) override;
    float tailMs() const override { return 2000.0f; }
private:
    float fs = 48000.0f;
    std::array<PitchVoice, 2> voice {};
    std::array<DelayLine, 2> vdelay {};    // per-voice delay (detune 0-50 ms, shifter 0-350 ms)
    std::array<float, 2> fbState {};
    std::array<Smoother, 2> ratioSm {};
    std::array<Smoother, 2> delaySm {};
};

// =========================================================================== Chorus / Flanger / Vibrato
class ChorusFlanger final : public FxBlock {
public:
    void prepare(float sr, int) override;
    void reset() override;
    void process(float* l, float* r, int n, const Ctx& c) override;
private:
    float fs = 48000.0f;
    std::array<DelayLine, 2> dl {};
    std::array<OnePoleLP, 2> hicut {}, fbCut {};
    std::array<float, 2> fb {};
    Lfo lfo;
    Smoother baseDelay, excursion;
};

// =========================================================================== Delay
class DelayBlock final : public FxBlock {
public:
    void prepare(float sr, int) override;
    void reset() override;
    void process(float* l, float* r, int n, const Ctx& c) override;
    float tailMs() const override { return 20000.0f; }
private:
    float lineTime(const Ctx& c, int timeP, int tempoP) const noexcept;
    float fs = 48000.0f;
    std::array<DelayLine, 2> dl {};
    std::array<OnePoleLP, 2> fbHi {};
    std::array<OnePoleHP, 2> fbLo {};
    std::array<Smoother, 2> time {};
    EnvFollower duck;
    float duckGainDb = 0.0f;
};

// =========================================================================== Reverb
class ReverbBlock final : public FxBlock {
public:
    void prepare(float sr, int) override;
    void reset() override;
    void process(float* l, float* r, int n, const Ctx& c) override;
    float tailMs() const override { return 25000.0f; }
    static constexpr int kLines = 8;
private:
    void configure(const Ctx& c);
    float fs = 48000.0f;
    DelayLine pre;
    std::array<DelayLine, 2> er {};
    std::array<DelayLine, kLines> line {};
    std::array<float, kLines> len {}, g {};
    std::array<OnePoleLP, kLines> damp {};
    std::array<Biquad, kLines> lowShelf {};
    struct Allpass { DelayLine d; float len = 100; float process(float x, float k) noexcept {
        const float v = d.read(len); const float u = x + k * v; d.push(u); return v - k * u; } };
    std::array<Allpass, 4> diffL {}, diffR {};
    std::array<Allpass, 6> spring {};       // dispersive chain for the Spring algorithm
    std::array<float, 12> erTapMs {}, erTapGain {};
    std::array<float, 15> cfg {};
    bool configured = false;
    Lfo lfo;
    float diffK = 0.6f;
    float springK = 0.0f;
};

} // namespace gmr::dsp
