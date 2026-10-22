#include "gmr/FactoryPresets.h"
#include <functional>
#include <initializer_list>
#include <utility>

namespace gmr {

namespace {
using KV = std::pair<int, int>;

int gLabelErrors = 0;
int chk(const ValueTable& t, const char* l) { const int v = tableStoredForLabel(t, l); if (v < 0) ++gLabelErrors; return v; }
int hc(const char* l) { return chk(tables::hiCut, l); }
int lc(const char* l) { return chk(tables::loCut, l); }
int sp(const char* l) { return chk(tables::speed, l); }
int eqf(const char* l) { return chk(tables::eqFreq, l); }
int eqw(const char* l) { return chk(tables::eqWidth, l); }
int ct(const char* l) { return chk(tables::compTime, l); }
int tt(const char* l) { return chk(tables::tapTempo, l); }

Patch make(const char* name, int userNumber, std::initializer_list<KV> kv,
           std::initializer_list<std::pair<int, Modifier>> mods = {}) {
    Patch p = initPatch();
    p.name = name;
    p.number = Patch::absoluteNumber(Bank::User, userNumber);
    for (const auto& e : kv) p.set(e.first, e.second);
    for (const auto& m : mods) if (Modifier* slot = p.modifierFor(m.first)) *slot = m.second;
    return p;
}
} // namespace

int factoryPresetLabelErrors() { gLabelErrors = 0; (void)factoryPresets(); return gLabelErrors; }

Patch initPatch() {
    Patch p;
    p.name = "Init";
    for (int b : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) p.set(b, 0);
    p.set(pOutLevel, -6);   // like the hardware factory bank: 6 dB of boost headroom (manual p.25)
    p.set(pRouting, 0);
    return p;
}

std::vector<Patch> factoryPresets() {
    const Modifier pedal { 1, 0, 50, 100 };          // M1, linear
    return {
        make("Clean Chorus Room", 1, { { pCompOn, 1 }, { pCompThreshold, -24 }, { pCompRatio, 7 }, { pCompGain, 3 },
             { pModOn, 1 }, { pModAlgo, algo::ClassicChorus }, { pModSpeed, sp(".750") }, { pModDepth, 45 }, { pModMix, 50 },
             { pRevOn, 1 }, { pRevAlgo, algo::Room }, { pRevDecay, 9 }, { pRevSize, 2 }, { pRevMix, 20 } }),
        make("Lead Delay Hall", 2, { { pCompOn, 1 }, { pCompThreshold, -18 }, { pCompRatio, 9 },
             { pDlyOn, 1 }, { pDlyAlgo, algo::Dual }, { pDlyTime, 440 }, { pDlyTime2, 660 }, { pDlyFeedback, 30 }, { pDlyFeedback2, 25 },
             { pDlyPan1, -40 }, { pDlyPan2, 40 }, { pDlyMix, 22 }, { pDlyFbHiCut, hc("4.47k") },
             { pRevOn, 1 }, { pRevAlgo, algo::Hall }, { pRevDecay, 28 }, { pRevSize, 5 }, { pRevMix, 22 }, { pRouting, 1 } }),
        make("Ambient Swell", 3, { { pModOn, 1 }, { pModAlgo, algo::AdvChorus }, { pModGoldRatio, 1 }, { pModPhReverse, 1 }, { pModMix, 40 },
             { pDlyOn, 1 }, { pDlyAlgo, algo::Dynamic }, { pDlyTime, 520 }, { pDlyOffset, 25 }, { pDlyFeedback, 55 },
             { pDlySense, -28 }, { pDlyDamp, 18 }, { pDlyMix, 40 },
             { pRevOn, 1 }, { pRevAlgo, algo::Hall }, { pRevDecay, 70 }, { pRevSize, 7 }, { pRevMix, 45 }, { pRevPreDelay, 40 }, { pRouting, 1 } }),
        make("Vintage Phaser", 4, { { pFltOn, 1 }, { pFltAlgo, algo::VintagePhaser }, { pFltSpeed, sp(".501") }, { pFltDepth, 70 },
             { pFltFeedback, 35 }, { pFltMix, 50 }, { pFltPhRange, 0 } }),
        make("Smooth Phase Lead", 5, { { pFltOn, 1 }, { pFltAlgo, algo::SmoothPhaser }, { pFltSpeed, sp(".251") }, { pFltDepth, 80 },
             { pFltFeedback, 20 }, { pFltMix, 50 }, { pFltPhRange, 1 }, { pFltPhReverse, 1 },
             { pDlyOn, 1 }, { pDlyTime, 380 }, { pDlyMix, 18 } }),
        make("Surf Tremolo Spring", 6, { { pFltOn, 1 }, { pFltAlgo, algo::Tremolo }, { pFltTempo, tt("1/8") }, { pFltDepth, 75 },
             { pFltTremType, 0 }, { pFltPulseWidth, 50 },
             { pRevOn, 1 }, { pRevAlgo, algo::Spring }, { pRevDecay, 22 }, { pRevSize, 2 }, { pRevMix, 35 }, { pTapTempo, 420 } }),
        make("Auto Wah Funk", 7, { { pCompOn, 1 }, { pCompThreshold, -26 }, { pCompRatio, 9 }, { pCompAttack, ct("3.0") },
             { pFltOn, 1 }, { pFltAlgo, algo::AutoRes }, { pFltSens, 7 }, { pFltResponse, 2 }, { pFltFreqMax, hc("3.55k") },
             { pFltOrder, 0 }, { pFltMix, 100 } }),
        make("Pedal Wah (M1)", 8, { { pFltOn, 1 }, { pFltAlgo, algo::Resonance }, { pFltResHiCut, hc("1.00k") }, { pFltResonance, 72 },
             { pFltOrder, 0 }, { pFltMix, 100 } }, { { pFltResHiCut, Modifier { 1, 15, 55, 85 } } }),
        make("Octave Down Bass", 9, { { pPitOn, 1 }, { pPitAlgo, algo::Octaver }, { pPitDirection, 0 }, { pPitRange, 1 }, { pPitMix, 55 },
             { pEqOn, 1 }, { pEq3Freq, eqf("3.98k") }, { pEq3Gain, -4 } }),
        make("Whammy Pedal (M1)", 10, { { pPitOn, 1 }, { pPitAlgo, algo::Whammy }, { pPitDirection, 1 }, { pPitRange, 1 }, { pWhamPitch, 0 } },
             { { pWhamPitch, pedal } }),
        make("Fifth Harmony", 11, { { pPitOn, 1 }, { pPitAlgo, algo::Shifter }, { pShfVoice1, 700 }, { pShfVoice2, 1200 },
             { pShfLevel1, 0 }, { pShfLevel2, -9 }, { pShfPan1, -25 }, { pShfPan2, 25 }, { pShfDelay1, 15 }, { pShfDelay2, 30 }, { pPitMix, 45 },
             { pRevOn, 1 }, { pRevMix, 20 } }),
        make("Wide Detune", 12, { { pPitOn, 1 }, { pPitAlgo, algo::Detune }, { pDetVoice1, 9 }, { pDetVoice2, -9 }, { pDetDelay1, 12 }, { pDetDelay2, 19 },
             { pPitMix, 45 }, { pRevOn, 1 }, { pRevAlgo, algo::Plate }, { pRevMix, 15 } }),
        make("Jet Flanger", 13, { { pModOn, 1 }, { pModAlgo, algo::AdvFlange }, { pModSpeed, sp(".200") }, { pModDepth, 85 }, { pModFeedback, 70 },
             { pModDelay, 15 }, { pModGoldRatio, 0 }, { pModMix, 50 }, { pModFbHiCut, hc("7.94k") } }),
        make("Vibrato", 14, { { pModOn, 1 }, { pModAlgo, algo::Vibrato }, { pModSpeed, sp("5.01") }, { pModDepth, 35 }, { pModHiCut, hc("8.91k") } }),
        make("Ping Pong 1/8D", 15, { { pDlyOn, 1 }, { pDlyAlgo, algo::PingPong }, { pDlyTempo, tt("1/8D") }, { pDlyFeedback, 40 }, { pDlyWidth, 100 },
             { pDlyMix, 30 }, { pDlyFbLoCut, lc("158.5") }, { pTapTempo, 480 } }),
        make("Slapback Dual", 16, { { pDlyOn, 1 }, { pDlyAlgo, algo::Dual }, { pDlyTime, 95 }, { pDlyTime2, 140 }, { pDlyFeedback, 5 }, { pDlyFeedback2, 5 },
             { pDlyPan1, -50 }, { pDlyPan2, 50 }, { pDlyMix, 35 }, { pDlyFbHiCut, hc("3.16k") } }),
        make("Plate Shimmer-ish", 17, { { pPitOn, 1 }, { pPitAlgo, algo::Shifter }, { pShfVoice1, 1200 }, { pShfLevel1, -12 }, { pShfLevel2, -100 },
             { pShfDelay1, 120 }, { pShfFb1, 45 }, { pPitMix, 30 },
             { pRevOn, 1 }, { pRevAlgo, algo::Plate }, { pRevDecay, 80 }, { pRevHiColor, 5 }, { pRevMix, 50 }, { pRouting, 2 } }),
        make("Compressed Clean", 18, { { pCompOn, 1 }, { pCompThreshold, -30 }, { pCompRatio, 10 }, { pCompAttack, ct("10") },
             { pCompRelease, ct("200") }, { pCompGain, 5 }, { pEqOn, 1 }, { pEq1Freq, eqf("125.9") }, { pEq1Gain, -3 },
             { pEq3Freq, eqf("5.01k") }, { pEq3Gain, 3 }, { pEq3Width, eqw("2.0") } }),
        make("Tight Rhythm Gate", 19, { { pGateOn, 1 }, { pGateMode, 1 }, { pGateThreshold, -42 }, { pGateMaxDamp, 60 }, { pGateRelease, 160 },
             { pEqOn, 1 }, { pEq2Freq, eqf("707.9") }, { pEq2Gain, -4 }, { pEq2Width, eqw("1.6") } }),
        make("Parallel Ambience", 20, { { pRouting, 2 }, { pModOn, 1 }, { pModAlgo, algo::ClassicChorus }, { pModMix, 30 },
             { pDlyOn, 1 }, { pDlyAlgo, algo::PingPong }, { pDlyTime, 375 }, { pDlyFeedback, 45 }, { pDlyMix, 30 },
             { pRevOn, 1 }, { pRevAlgo, algo::Hall }, { pRevDecay, 45 }, { pRevMix, 35 } }),
    };
}

} // namespace gmr
