#include "gmr/Engine.h"
#include <cstring>

namespace gmr {

namespace {
uint32_t packMod(const Modifier& m) noexcept {
    return (uint32_t)m.source | ((uint32_t)m.minOut << 8) | ((uint32_t)m.midOut << 16) | ((uint32_t)m.maxOut << 24);
}
Modifier unpackMod(uint32_t u) noexcept {
    Modifier m;
    m.source = (uint8_t)(u & 0xFF); m.minOut = (uint8_t)((u >> 8) & 0xFF);
    m.midOut = (uint8_t)((u >> 16) & 0xFF); m.maxOut = (uint8_t)((u >> 24) & 0xFF);
    return m;
}
} // namespace

float modifierCurve(const Modifier& m, float u) noexcept {
    u = dsp::clampf(u, 0.0f, 1.0f);
    const float b0 = 2.0f * (u - 0.5f) * (u - 1.0f);
    const float b1 = -4.0f * u * (u - 1.0f);
    const float b2 = 2.0f * u * (u - 0.5f);
    return dsp::clampf(b0 * m.minOut + b1 * m.midOut + b2 * m.maxOut, 0.0f, 100.0f);
}

Engine::Engine() {
    for (int i = 0; i < kNumParams; ++i) target[(size_t)i].store(paramDef(i).def);
    for (auto& m : modPacked) m.store(packMod(Modifier{}));
    for (int i = 0; i < kNumParams; ++i) eff[(size_t)i] = (float)paramDef(i).def;

    dsp::FxBlock* fx[8] = { &gate, &eq, &comp, &filter, &pitch, &mod, &delay, &reverb };
    const Block blocks[8] = { Block::Gate, Block::Eq, Block::Comp, Block::Filter, Block::Pitch, Block::Mod, Block::Delay, Block::Reverb };
    const int on[8]  = { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn };
    const int mix[8] = { -1, -1, -1, pFltMix, pPitMix, pModMix, pDlyMix, pRevMix };
    const int out[8] = { -1, -1, -1, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut };
    for (int i = 0; i < 8; ++i) {
        slots[(size_t)i].fx = fx[i]; slots[(size_t)i].block = blocks[i];
        slots[(size_t)i].onP = on[i]; slots[(size_t)i].mixP = mix[i]; slots[(size_t)i].outP = out[i];
        slots[(size_t)i].algoP = algoParamFor(blocks[i]);
    }
}

void Engine::prepare(double sampleRate, int) {
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& s : slots) s.fx->prepare((float)fs, kSubBlock);
    rampStep = 1.0f / (0.015f * (float)fs);
    reset();
}

void Engine::reset() {
    for (auto& s : slots) {
        s.fx->reset();
        s.onGain = target[(size_t)s.onP].load() ? 1.0f : 0.0f;
        s.wasOn = s.onGain > 0.5f;
        s.algoGain = 1.0f;
        s.lastAlgo = s.algoP >= 0 ? target[(size_t)s.algoP].load() : 0;
    }
    activeRouting = target[pRouting].load();
    routeGain = 1.0f;
    bypassGain = target[pBypass].load() ? 1.0f : 0.0f;
}

void Engine::setParam(int p, int stored) noexcept {
    if (p < 0 || p >= kNumParams) return;
    const ParamDef& d = paramDef(p);
    target[(size_t)p].store(std::clamp(stored, d.lo, d.hi), std::memory_order_relaxed);
}

void Engine::setModifier(int slot, const Modifier& m) noexcept {
    if (slot < 0 || slot >= kNumModSlots) return;
    Modifier c = m;
    c.source = (uint8_t)std::min<int>(c.source, 4);
    c.minOut = (uint8_t)std::min<int>(c.minOut, 100);
    c.midOut = (uint8_t)std::min<int>(c.midOut, 100);
    c.maxOut = (uint8_t)std::min<int>(c.maxOut, 100);
    modPacked[(size_t)slot].store(packMod(c), std::memory_order_relaxed);
}

Modifier Engine::getModifier(int slot) const noexcept {
    if (slot < 0 || slot >= kNumModSlots) return {};
    return unpackMod(modPacked[(size_t)slot].load(std::memory_order_relaxed));
}

void Engine::loadPatch(const Patch& p) noexcept {
    for (int i = 0; i < pInLevel; ++i) setParam(i, p.get(i));
    for (int s = 0; s < kNumModSlots; ++s) setModifier(s, p.mods[(size_t)s]);
}

void Engine::storeInto(Patch& p) const noexcept {
    for (int i = 0; i < kNumParams; ++i) p.v[(size_t)i] = getParam(i);
    for (int s = 0; s < kNumModSlots; ++s) p.mods[(size_t)s] = getModifier(s);
}

void Engine::computeEffective() noexcept {
    for (int i = 0; i < kNumParams; ++i) eff[(size_t)i] = (float)target[(size_t)i].load(std::memory_order_relaxed);
    int count = 0;
    const ModSlot* ms = modSlots(count);
    for (int i = 0; i < count; ++i) {
        const ModSlot& s = ms[i];
        const int ap = algoParamFor(s.block);
        if (ap < 0 || (int)eff[(size_t)ap] != s.algo) continue;
        const Modifier m = unpackMod(modPacked[(size_t)((s.offset - kFirstModOffset) / 4)].load(std::memory_order_relaxed));
        if (m.source < 1 || m.source > 4) continue;
        const float in = eff[(size_t)(pMod1 + m.source - 1)] / 127.0f;
        // Modifiers sweep the HARDWARE range (for OUT LEVELs: -100 dB = Off .. 0 dB), so an expression
        // pedal behaves like on the G-Major and can never push a level to +50 dB.
        int lo, hi; hardwareRange(s.param, lo, hi);
        eff[(size_t)s.param] = (float)lo + modifierCurve(m, in) * 0.01f * (float)(hi - lo);
    }
}

float Engine::mixOf(const Slot& s) const noexcept {
    if (s.mixP < 0) return 1.0f;
    const int a = s.algoP >= 0 ? (int)eff[(size_t)s.algoP] : 0;
    if (!paramUsedBy(paramDef(s.mixP), a)) return 1.0f;      // e.g. Tremolo, Panner, Vibrato, Whammy
    return dsp::clampf(eff[(size_t)s.mixP] * 0.01f, 0.0f, 1.0f);
}

float Engine::outOf(const Slot& s) const noexcept {
    return s.outP < 0 ? 1.0f : dsp::dbToGain(eff[(size_t)s.outP]);
}

bool Engine::beginSlot(Slot& s, int n, float& g0, float& step) noexcept {
    const bool on = eff[(size_t)s.onP] > 0.5f;
    if (on && !s.wasOn && s.onGain <= 0.0f) s.fx->reset();   // no stale tails on re-enable
    s.wasOn = on;
    const int a = s.algoP >= 0 ? (int)eff[(size_t)s.algoP] : 0;
    if (a != s.lastAlgo) {                                   // algorithm switch: reset + fade in
        s.lastAlgo = a;
        s.fx->reset();
        s.algoGain = 0.0f;
    }
    g0 = s.onGain * s.algoGain;
    const float inc = rampStep * (float)n;
    s.onGain = on ? std::min(1.0f, s.onGain + inc) : std::max(0.0f, s.onGain - inc);
    s.algoGain = std::min(1.0f, s.algoGain + inc);
    const float g1 = s.onGain * s.algoGain;
    step = (g1 - g0) / (float)n;
    return g0 > 0.0f || g1 > 0.0f;
}

void Engine::runSerial(Slot& s, float* l, float* r, int n, const dsp::Ctx& c) noexcept {
    float g0, st;
    if (!beginSlot(s, n, g0, st)) return;
    std::memcpy(dryL, l, sizeof(float) * (size_t)n); std::memcpy(dryR, r, sizeof(float) * (size_t)n);
    s.fx->process(l, r, n, c);                                 // l/r now 100 % wet
    const float mix = mixOf(s), out = outOf(s);
    const bool hasMix = s.mixP >= 0 && paramUsedBy(paramDef(s.mixP), s.algoP >= 0 ? (int)eff[(size_t)s.algoP] : 0);
    const float dryG = hasMix ? (c.killDry ? 0.0f : 1.0f - mix) : 0.0f;
    for (int k = 0; k < n; ++k) {
        const float g = g0 + st * (float)k;
        const float yl = out * (dryG * dryL[k] + mix * l[k]);
        const float yr = out * (dryG * dryR[k] + mix * r[k]);
        l[k] = dryL[k] + (yl - dryL[k]) * g;
        r[k] = dryR[k] + (yr - dryR[k]) * g;
    }
}

// Parallel group: the input passes once (unless Kill Dry); every active block adds its
// wet signal scaled by Mix x Out Level — the "mixer loop" behaviour described on p.19/21.
void Engine::runParallel(Slot** group, int count, float* l, float* r, int n, const dsp::Ctx& c) noexcept {
    std::memcpy(inL, l, sizeof(float) * (size_t)n); std::memcpy(inR, r, sizeof(float) * (size_t)n);
    const float dg = c.killDry ? 0.0f : 1.0f;
    for (int k = 0; k < n; ++k) { accL[k] = inL[k] * dg; accR[k] = inR[k] * dg; }
    for (int i = 0; i < count; ++i) {
        Slot& s = *group[i];
        float g0, st;
        if (!beginSlot(s, n, g0, st)) continue;
        std::memcpy(bufL, inL, sizeof(float) * (size_t)n); std::memcpy(bufR, inR, sizeof(float) * (size_t)n);
        s.fx->process(bufL, bufR, n, c);
        const float w = mixOf(s) * outOf(s);
        for (int k = 0; k < n; ++k) {
            const float g = (g0 + st * (float)k) * w;
            accL[k] += bufL[k] * g; accR[k] += bufR[k] * g;
        }
    }
    std::memcpy(l, accL, sizeof(float) * (size_t)n); std::memcpy(r, accR, sizeof(float) * (size_t)n);
}

void Engine::process(float* left, float* right, int numSamples) noexcept {
    float inMax = 0.0f, outMax = 0.0f;
    for (int pos = 0; pos < numSamples; pos += kSubBlock) {
        const int n = std::min(kSubBlock, numSamples - pos);
        float* l = left + pos;
        float* r = right + pos;
        computeEffective();

        dsp::Ctx c;
        c.sr = (float)fs;
        c.s = eff.data();
        c.killDry = eff[pKillDry] > 0.5f;
        const float bpm = hostBpm.load(std::memory_order_relaxed);
        c.beatMs = (eff[pTempoSource] > 0.5f && bpm > 1.0f) ? 60000.0f / bpm : eff[pTapTempo];

        // input stage
        const float inG = dsp::dbToGain(eff[pInLevel]);
        float origL[kSubBlock], origR[kSubBlock];
        for (int k = 0; k < n; ++k) {
            origL[k] = l[k]; origR[k] = r[k];
            l[k] *= inG; r[k] *= inG;
            inMax = std::max(inMax, std::max(std::fabs(l[k]), std::fabs(r[k])));
        }

        // routing change => short dip to avoid a topology click
        const int wantRouting = (int)eff[pRouting];
        const float rampN = rampStep * (float)n * 3.0f;
        const float rg0 = routeGain;
        if (wantRouting != activeRouting) {
            routeGain = std::max(0.0f, routeGain - rampN);
            if (routeGain <= 0.0f) activeRouting = wantRouting;
        } else routeGain = std::min(1.0f, routeGain + rampN);

        // Fixed serial front end: Gate -> EQ -> Comp -> Filter/Mod (manual p.21 diagrams;
        // EQ position after the gate is inferred — it shares the gate key on the unit).
        for (int i = 0; i < 4; ++i) runSerial(slots[(size_t)i], l, r, n, c);
        Slot* tail[4] = { &slots[4], &slots[5], &slots[6], &slots[7] };   // pitch, mod, delay, reverb
        if (activeRouting == 0) {
            for (Slot* s : tail) runSerial(*s, l, r, n, c);
        } else if (activeRouting == 1) {
            runSerial(*tail[0], l, r, n, c);
            runSerial(*tail[1], l, r, n, c);
            runParallel(tail + 2, 2, l, r, n, c);
        } else {
            runParallel(tail, 4, l, r, n, c);
        }

        // preset out level, global out, bypass (bypass passes the unprocessed input)
        const float outG = dsp::dbToGain(eff[pOutLevel]) * dsp::dbToGain(eff[pGlobalOut]);
        const float b0 = bypassGain;
        bypassGain = eff[pBypass] > 0.5f ? std::min(1.0f, bypassGain + rampStep * (float)n)
                                         : std::max(0.0f, bypassGain - rampStep * (float)n);
        bool bad = false;
        for (int k = 0; k < n; ++k) {
            const float t = (float)k / (float)n;
            const float rg = rg0 + (routeGain - rg0) * t;
            const float bg = b0 + (bypassGain - b0) * t;
            const float pl = l[k] * outG * rg, pr = r[k] * outG * rg;
            l[k] = pl + (origL[k] - pl) * bg;
            r[k] = pr + (origR[k] - pr) * bg;
            if (!std::isfinite(l[k]) || !std::isfinite(r[k])) bad = true;
            outMax = std::max(outMax, std::max(std::fabs(l[k]), std::fabs(r[k])));
        }
        if (bad) {   // never let NaN/Inf reach the speakers: mute this block and clear state
            for (int k = 0; k < n; ++k) { l[k] = 0.0f; r[k] = 0.0f; }
            for (auto& s : slots) s.fx->reset();
            nanResets.fetch_add(1);
            outMax = 0.0f;
        }
    }
    const float decay = std::exp(-(float)numSamples / (0.25f * (float)fs));
    inPeak.store(std::max(inMax, inPeak.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    outPeak.store(std::max(outMax, outPeak.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    gateDb.store(gate.currentGainDb(), std::memory_order_relaxed);
    compGr.store(comp.gainReductionDb(), std::memory_order_relaxed);
}

} // namespace gmr
