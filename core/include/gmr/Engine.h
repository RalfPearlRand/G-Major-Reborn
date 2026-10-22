// G-Major Reborn — real-time engine.
//
// Thread model: any thread may call setParam()/setModifier() (lock-free atomics);
// only the audio thread calls process(). process() never allocates, locks or does I/O.
#pragma once
#include "gmr/Patch.h"
#include "gmr/dsp/Blocks.h"
#include <atomic>
#include <memory>

namespace gmr {

class Engine {
public:
    static constexpr int kSubBlock = 32;   // parameter / modifier update rate

    Engine();
    void prepare(double sampleRate, int maxBlockSize);   // allocates; not real-time safe
    void reset();                                        // clears all DSP state

    // ---- parameters (thread-safe) ----
    void setParam(int p, int stored) noexcept;
    int getParam(int p) const noexcept { return target[(size_t)p].load(std::memory_order_relaxed); }
    void setModifier(int slot, const Modifier& m) noexcept;
    Modifier getModifier(int slot) const noexcept;
    /** Applies every preset parameter + modifier of `p` (live/unit settings untouched). */
    void loadPatch(const Patch& p) noexcept;
    /** Copies current preset parameters + modifiers into `p` (keeps p.name/number/raw). */
    void storeInto(Patch& p) const noexcept;
    void setHostTempo(double bpm, bool valid) noexcept { hostBpm.store(valid ? (float)bpm : 0.0f); }

    // ---- audio ----
    void process(float* left, float* right, int numSamples) noexcept;

    // ---- metering (read from any thread) ----
    float inputPeak() const noexcept { return inPeak.load(std::memory_order_relaxed); }
    float outputPeak() const noexcept { return outPeak.load(std::memory_order_relaxed); }
    float gateGainDb() const noexcept { return gateDb.load(std::memory_order_relaxed); }
    float compReductionDb() const noexcept { return compGr.load(std::memory_order_relaxed); }
    int resetsAfterNonFinite() const noexcept { return nanResets.load(); }
    double sampleRate() const noexcept { return fs; }
    /** Effective (modifier-applied) stored-domain value seen by the DSP, for UI/tests. */
    float effective(int p) const noexcept { return eff[(size_t)p]; }

private:
    struct Slot {
        dsp::FxBlock* fx = nullptr;
        Block block = Block::Gate;
        int onP = -1, mixP = -1, outP = -1, algoP = -1;
        float onGain = 0.0f;      // 0..1 linear ramp
        float algoGain = 1.0f;    // fade-in after an algorithm change
        int lastAlgo = -1;
        bool wasOn = false;
    };
    void computeEffective() noexcept;
    void runSerial(Slot& s, float* l, float* r, int n, const dsp::Ctx& c) noexcept;
    void runParallel(Slot** slots, int count, float* l, float* r, int n, const dsp::Ctx& c) noexcept;
    bool beginSlot(Slot& s, int n, float& g0, float& step) noexcept;
    float mixOf(const Slot& s) const noexcept;
    float outOf(const Slot& s) const noexcept;

    double fs = 48000.0;
    std::array<std::atomic<int>, kNumParams> target;
    std::array<std::atomic<uint32_t>, kNumModSlots> modPacked;
    std::array<float, kNumParams> eff {};
    std::atomic<float> hostBpm { 0.0f };

    dsp::NoiseGate gate; dsp::Equalizer eq; dsp::Compressor comp; dsp::FilterMod filter;
    dsp::PitchBlock pitch; dsp::ChorusFlanger mod; dsp::DelayBlock delay; dsp::ReverbBlock reverb;
    std::array<Slot, 8> slots {};

    float bufL[kSubBlock] {}, bufR[kSubBlock] {}, dryL[kSubBlock] {}, dryR[kSubBlock] {};
    float accL[kSubBlock] {}, accR[kSubBlock] {}, inL[kSubBlock] {}, inR[kSubBlock] {};
    int activeRouting = 0;
    float routeGain = 1.0f, bypassGain = 0.0f;
    float rampStep = 0.0f;  // per-sample increment for 15 ms ramps

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, gateDb { 0.0f }, compGr { 0.0f };
    std::atomic<int> nanResets { 0 };
};

/** Modifier response curve: quadratic through (0,min) (0.5,mid) (1,max), result 0..100 %. */
float modifierCurve(const Modifier& m, float input01) noexcept;

} // namespace gmr
