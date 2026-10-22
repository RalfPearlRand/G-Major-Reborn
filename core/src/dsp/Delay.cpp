// DELAY block: Ping Pong, Dynamic (ducking, TC 2290 style), Dual (manual pages 37-39).
#include "gmr/dsp/Blocks.h"

namespace gmr::dsp {

namespace {
inline float safety(float x) noexcept {
    return std::fabs(x) < 1.0f ? x : std::copysign(1.0f + std::tanh(std::fabs(x) - 1.0f), x);
}
constexpr float kMaxDelayMs = 1800.0f;
}

void DelayBlock::prepare(float sr, int) {
    fs = sr;
    for (auto& d : dl) d.allocate((int)(sr * 2.1f) + 8);
    for (auto& t : time) t.setTime(70.0f, sr);
    reset();
}

void DelayBlock::reset() {
    for (auto& d : dl) d.reset();
    for (auto& f : fbHi) f.reset();
    for (auto& f : fbLo) f.reset();
    for (auto& t : time) t.reset(-1.0f);
    duck.reset();
    duckGainDb = 0.0f;
}

float DelayBlock::lineTime(const Ctx& c, int timeP, int tempoP) const noexcept {
    float ms = c.tempoMs(tempoP);                 // Tempo overrides Time unless "Ignored"
    if (ms <= 0.0f) ms = c.val(timeP);
    return clampf(ms, 0.0f, kMaxDelayMs) * 0.001f * fs;
}

void DelayBlock::process(float* l, float* r, int n, const Ctx& c) {
    const int a = c.i(pDlyAlgo);
    const float hc = c.val(pDlyFbHiCut), lc = c.val(pDlyFbLoCut);
    for (int ch = 0; ch < 2; ++ch) { fbHi[(size_t)ch].setCutoff(hc, fs); fbLo[(size_t)ch].setCutoff(lc, fs); }
    const float fb1 = clampf(c.val(pDlyFeedback) * 0.01f, 0.0f, 1.0f) * 0.99f;

    float t0 = lineTime(c, pDlyTime, pDlyTempo), t1 = t0;
    if (a == algo::Dynamic) t1 = clampf(t0 + c.val(pDlyOffset) * 0.001f * fs, 0.0f, 2.0f * fs);
    if (a == algo::Dual) t1 = lineTime(c, pDlyTime2, pDlyTempo2);
    time[0].target = t0; time[1].target = t1;
    if (time[0].cur < 0.0f) { time[0].reset(t0); time[1].reset(t1); }

    if (a == algo::PingPong) {
        // Mono in, repeats alternate L -> R -> L ...; Width 100 % = hard panned repeats.
        const float w = c.val(pDlyWidth) * 0.01f;
        const float own = 0.5f + 0.5f * w, other = 0.5f - 0.5f * w;
        for (int k = 0; k < n; ++k) {
            const float d0 = 1.0f + time[0].next(); time[1].next();
            const float yl = dl[0].read(d0), yr = dl[1].read(d0);
            const float m = 0.5f * (l[k] + r[k]);
            dl[0].push(safety(m + fb1 * fbLo[0].process(fbHi[0].process(yr))));
            dl[1].push(safety(fb1 * fbLo[1].process(fbHi[1].process(yl))));
            l[k] = yl * own + yr * other;
            r[k] = yr * own + yl * other;
        }
        return;
    }

    if (a == algo::Dynamic) {
        // Stereo delay, right repeats offset by Offset R. Repeats are "ducked" by Damping dB
        // while the input exceeds Sensitivity, and recover with the Release time.
        const float sens = c.val(pDlySense), dampDb = c.val(pDlyDamp);
        const float relMs = std::max(1.0f, c.val(pDlyRelease));
        const float attStep = 1.0f - std::exp(-1.0f / (0.010f * fs));      // 10 ms duck-in (inferred)
        const float relStep = 1.0f - std::exp(-1.0f / (relMs * 0.001f * fs));
        duck.set(1.0f, 60.0f, fs);
        for (int k = 0; k < n; ++k) {
            const float d0 = 1.0f + time[0].next(), d1 = 1.0f + time[1].next();
            const float yl = dl[0].read(d0), yr = dl[1].read(d1);
            const float lev = gainToDb(duck.process(std::max(std::fabs(l[k]), std::fabs(r[k]))));
            const float target = lev > sens ? -dampDb : 0.0f;
            duckGainDb += (target - duckGainDb) * (target < duckGainDb ? attStep : relStep);
            dl[0].push(safety(l[k] + fb1 * fbLo[0].process(fbHi[0].process(yl))));
            dl[1].push(safety(r[k] + fb1 * fbLo[1].process(fbHi[1].process(yr))));
            const float g = dbToGain(duckGainDb);
            l[k] = yl * g; r[k] = yr * g;
        }
        return;
    }

    // Dual: two independent mono-fed lines, each with its own time/tempo/feedback/pan.
    const float fb2 = clampf(c.val(pDlyFeedback2) * 0.01f, 0.0f, 1.0f) * 0.99f;
    float gl[2], gr[2];
    panGains(c.val(pDlyPan1) / 50.0f, gl[0], gr[0]);
    panGains(c.val(pDlyPan2) / 50.0f, gl[1], gr[1]);
    const float fbs[2] = { fb1, fb2 };
    for (int k = 0; k < n; ++k) {
        const float m = 0.5f * (l[k] + r[k]);
        const float d[2] = { 1.0f + time[0].next(), 1.0f + time[1].next() };
        float ol = 0.0f, orr = 0.0f;
        for (int v = 0; v < 2; ++v) {
            const float y = dl[(size_t)v].read(d[v]);
            dl[(size_t)v].push(safety(m + fbs[v] * fbLo[(size_t)v].process(fbHi[(size_t)v].process(y))));
            ol += y * gl[v]; orr += y * gr[v];
        }
        l[k] = ol; r[k] = orr;
    }
}

} // namespace gmr::dsp
