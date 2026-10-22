// PITCH block: Detune, Whammy, Octaver, Pitch Shifter (manual pages 40-41).
// The G-Major's pitch engine is proprietary; this is a classic two-head rotating-tap
// time-domain shifter. It is mono-in (L+R)/2, like most hardware pitch blocks.
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

void PitchVoice::prepare(float sr, float windowMs) {
    fs = sr;
    dl.allocate((int)(sr * 0.2f) + 8);   // up to 200 ms windows
    setWindowMs(windowMs);
    reset();
}
void PitchVoice::reset() { dl.reset(); phase = 0.0f; }
void PitchVoice::setWindowMs(float ms) noexcept { win = clampf(ms * 0.001f * fs, 64.0f, (float)dl.capacity() - 8.0f); }

float PitchVoice::process(float x, float ratio) noexcept {
    dl.push(x);
    phase += (1.0f - ratio) / win;
    phase -= std::floor(phase);
    float p2 = phase + 0.5f; p2 -= std::floor(p2);
    const float s1 = std::sin(kPi * phase), s2 = std::sin(kPi * p2);
    return dl.read(2.0f + phase * win) * s1 * s1 + dl.read(2.0f + p2 * win) * s2 * s2;
}

void PitchBlock::prepare(float sr, int) {
    fs = sr;
    for (auto& v : voice) v.prepare(sr, 40.0f);
    for (auto& d : vdelay) d.allocate((int)(sr * 0.36f) + 8);   // 350 ms + margin
    for (auto& s : ratioSm) s.setTime(25.0f, sr);
    for (auto& s : delaySm) s.setTime(60.0f, sr);
    reset();
}

void PitchBlock::reset() {
    for (auto& v : voice) v.reset();
    for (auto& d : vdelay) d.reset();
    fbState = { 0.0f, 0.0f };
    for (auto& s : ratioSm) s.reset(0.0f);
    for (auto& s : delaySm) s.reset(0.0f);
}

void PitchBlock::process(float* l, float* r, int n, const Ctx& c) {
    const int a = c.i(pPitAlgo);
    const float sr = fs;

    if (a == algo::Detune) {
        // Two voices, each detuned ±100 cents and delayed 0-50 ms; voice 1 left, voice 2 right.
        voice[0].setWindowMs(25.0f); voice[1].setWindowMs(25.0f);
        ratioSm[0].target = c.val(pDetVoice1); ratioSm[1].target = c.val(pDetVoice2);
        delaySm[0].target = c.val(pDetDelay1) * 0.001f * sr; delaySm[1].target = c.val(pDetDelay2) * 0.001f * sr;
        for (int k = 0; k < n; ++k) {
            const float m = 0.5f * (l[k] + r[k]);
            float y[2];
            for (int v = 0; v < 2; ++v) {
                const float ratio = std::exp2(ratioSm[(size_t)v].next() / 1200.0f);
                vdelay[(size_t)v].push(voice[(size_t)v].process(m, ratio));
                y[v] = vdelay[(size_t)v].read(1.0f + delaySm[(size_t)v].next());
            }
            l[k] = y[0]; r[k] = y[1];
        }
        return;
    }

    if (a == algo::Shifter) {
        // Two voices ±2400 cents, pan, 0-350 ms delay with regenerating feedback, level.
        voice[0].setWindowMs(45.0f); voice[1].setWindowMs(45.0f);
        const int vp[2] = { pShfVoice1, pShfVoice2 }, pp[2] = { pShfPan1, pShfPan2 };
        const int dp[2] = { pShfDelay1, pShfDelay2 }, fp[2] = { pShfFb1, pShfFb2 }, lp[2] = { pShfLevel1, pShfLevel2 };
        float gl[2], gr[2], lev[2], fb[2];
        for (int v = 0; v < 2; ++v) {
            ratioSm[(size_t)v].target = c.val(vp[v]);
            delaySm[(size_t)v].target = c.val(dp[v]) * 0.001f * sr;
            panGains(c.val(pp[v]) / 50.0f, gl[v], gr[v]);
            lev[v] = dbToGain(c.val(lp[v]));
            fb[v] = clampf(c.val(fp[v]) * 0.01f, 0.0f, 1.0f) * 0.95f;
        }
        for (int k = 0; k < n; ++k) {
            const float m = 0.5f * (l[k] + r[k]);
            float ol = 0.0f, orr = 0.0f;
            for (int v = 0; v < 2; ++v) {
                const float ratio = std::exp2(ratioSm[(size_t)v].next() / 1200.0f);
                const float s = voice[(size_t)v].process(m + fb[v] * fbState[(size_t)v], ratio);
                vdelay[(size_t)v].push(s);
                const float y = vdelay[(size_t)v].read(1.0f + delaySm[(size_t)v].next());
                fbState[(size_t)v] = flushDenormal(y);
                ol += y * gl[v] * lev[v]; orr += y * gr[v] * lev[v];
            }
            l[k] = ol; r[k] = orr;
        }
        return;
    }

    // Octaver / Whammy: single voice, Direction Up/Down, Range 1 or 2 octaves.
    const float dir = c.i(pPitDirection) == 1 ? 1.0f : -1.0f;
    const float range = (float)std::clamp(c.i(pPitRange), 1, 2);
    float cents;
    if (a == algo::Octaver) cents = dir * range * 1200.0f;
    else cents = dir * range * 1200.0f * c.val(pWhamPitch) * 0.01f;   // Whammy: Pitch = pedal position
    voice[0].setWindowMs(range > 1.5f ? 60.0f : 45.0f);
    ratioSm[0].target = cents;
    for (int k = 0; k < n; ++k) {
        const float m = 0.5f * (l[k] + r[k]);
        const float y = voice[0].process(m, std::exp2(ratioSm[0].next() / 1200.0f));
        l[k] = y; r[k] = y;
    }
}

} // namespace gmr::dsp
