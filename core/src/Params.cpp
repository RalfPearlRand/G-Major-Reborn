#include "gmr/Params.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace gmr {
namespace {

constexpr uint32_t ALL = 0xFFFFFFFFu;
constexpr uint32_t b(int a) { return 1u << a; }

const char* const kOffOn[]     = { "Off", "On" };
const char* const kRouting[]   = { "Serial", "Semi Parallel", "Parallel" };
const char* const kGateMode[]  = { "Soft", "Hard" };
const char* const kOrder[]     = { "2nd", "4th" };
const char* const kResponse[]  = { "Slow", "Medium", "Fast" };
const char* const kPhRange[]   = { "Low", "High" };
const char* const kTremType[]  = { "Soft", "Hard" };
const char* const kDirection[] = { "Down", "Up" };
const char* const kOctRange[]  = { "1 Oct", "2 Oct" };
const char* const kShape[]     = { "Round", "Curved", "Square" };
const char* const kTempoSrc[]  = { "Tap", "Host" };

const char* const kFilterAlgos[] = { "AUTO RESONANCE", "RESONANCE FILTER", "VINTAGE PHASER", "SMOOTH PHASER", "TREMOLO", "PANNER" };
const char* const kPitchAlgos[]  = { "DETUNE", "WHAMMY", "OCTAVER", "PITCH SHIFTER" };
const char* const kModAlgos[]    = { "CLASSIC CHORUS", "ADVANCED CHORUS", "CLASSIC FLANGE", "ADVANCED FLANGE", "VIBRATO" };
const char* const kDelayAlgos[]  = { "PING PONG", "DYNAMIC", "DUAL" };
const char* const kReverbAlgos[] = { "SPRING", "HALL", "ROOM", "PLATE" };

using namespace algo;
const uint32_t kPhasers = b(VintagePhaser) | b(SmoothPhaser);
const uint32_t kLfoFilt = kPhasers | b(Tremolo) | b(Panner);

#define I(id,key,lab,blk,mask,lo,hi,def,unit,off,enc)  { id, key, lab, Block::blk, mask, Kind::Int,    lo, hi, def, nullptr, nullptr, unit, off, Enc::enc }
#define T(id,key,lab,blk,mask,tab,lo,hi,def,unit,off)  { id, key, lab, Block::blk, mask, Kind::Table,  lo, hi, def, &tables::tab, nullptr, unit, off, Enc::Plain }
#define C(id,key,lab,blk,mask,ch,lo,hi,def,off,enc)    { id, key, lab, Block::blk, mask, Kind::Choice, lo, hi, def, nullptr, ch, "", off, Enc::enc }
#define B(id,key,lab,blk,mask,def,off,enc)             { id, key, lab, Block::blk, mask, Kind::Bool,   0, 1, def, nullptr, kOffOn, "", off, Enc::enc }

// Stored-domain table ranges (absolute stored value = table index + storeOffset):
//   compTime   idx0 "1.0" .. idx20 "2000", storeOffset 3
//   hiCut      idx0 "19.95" .. idx60 "Off";  "158.5"=18 "14.1k"=57 "1.00k"=34 "10.0k"=54 "2.00k"=40
//   loCut      idx0 "Off" .. idx40 "2.00k"
//   eqFreq     idx0 "40.97" .. idx216 "Off", storeOffset 25  -> 25..241
//   eqWidth    idx0 "0.2" .. idx13 "4.0", storeOffset 3      -> 3..16
const ParamDef kParams[kNumParams] = {
    // ------------------------------------------------------------- Global (patch)
    C(pRouting,  "routing",  "ROUTING",   Global, ALL, kRouting, 0, 2, 0, 156, Special),
    I(pOutLevel, "out_level","OUT LEVEL", Global, ALL, -50, 50, -6, "dB", 156, Special),
    B(pRelay1,   "relay1",   "RELAY 1",   Global, ALL, 0, 156, Special),
    B(pRelay2,   "relay2",   "RELAY 2",   Global, ALL, 0, 156, Special),
    I(pTapTempo, "tap_tempo","TAP TEMPO", Global, ALL, 100, 3000, 500, "ms", 160, Plain),
    // ------------------------------------------------------------- Noise gate
    B(pGateOn,        "gate_on",   "NOISE GATE", Gate, ALL, 0, 608, BoolInv),
    C(pGateMode,      "gate_mode", "GATE TYPE",  Gate, ALL, kGateMode, 0, 1, 0, 548, Plain),
    I(pGateThreshold, "gate_thr",  "THRESHOLD",  Gate, ALL, -60, 0, -50, "dB", 552, Plain),
    I(pGateMaxDamp,   "gate_damp", "MAX DAMP",   Gate, ALL, 0, 90, 40, "dB", 556, Plain),
    I(pGateRelease,   "gate_rel",  "RELEASE",    Gate, ALL, 3, 200, 120, "dB/sec", 560, Plain),
    // ------------------------------------------------------------- EQ (on flag: 1 = ON, unlike the others)
    B(pEqOn,     "eq_on",     "EQUALIZER", Eq, ALL, 0, 564, Plain),
    T(pEq1Freq,  "eq1_freq",  "FREQ",  Eq, ALL, eqFreq, 25, 241, 25 + 63,  "Hz", 568),   // 251.2 Hz
    I(pEq1Gain,  "eq1_gain",  "GAIN",  Eq, ALL, -12, 12, 0, "dB", 572, Plain),
    T(pEq1Width, "eq1_width", "WIDTH", Eq, ALL, eqWidth, 3, 16, 3 + 7, "oct", 576),        // 1.0 oct
    T(pEq2Freq,  "eq2_freq",  "FREQ",  Eq, ALL, eqFreq, 25, 241, 25 + 119, "Hz", 580),    // 1.26 kHz
    I(pEq2Gain,  "eq2_gain",  "GAIN",  Eq, ALL, -12, 12, 0, "dB", 584, Plain),
    T(pEq2Width, "eq2_width", "WIDTH", Eq, ALL, eqWidth, 3, 16, 3 + 7, "oct", 588),
    T(pEq3Freq,  "eq3_freq",  "FREQ",  Eq, ALL, eqFreq, 25, 241, 25 + 167, "Hz", 592),   // 5.01 kHz
    I(pEq3Gain,  "eq3_gain",  "GAIN",  Eq, ALL, -12, 12, 0, "dB", 596, Plain),
    T(pEq3Width, "eq3_width", "WIDTH", Eq, ALL, eqWidth, 3, 16, 3 + 7, "oct", 600),
    // ------------------------------------------------------------- Compressor
    B(pCompOn,        "comp_on",      "COMPRESSOR", Comp, ALL, 0, 224, BoolInv),
    I(pCompThreshold, "comp_thr",     "THRESHOLD",  Comp, ALL, -40, 0, -20, "dB", 164, Plain),
    T(pCompRatio,     "comp_ratio",   "RATIO",      Comp, ALL, compRatio, 0, 15, 6, "", 168),       // 2.0:1
    T(pCompAttack,    "comp_attack",  "ATTACK",     Comp, ALL, compTime, 3, 14, 3 + 6, "ms", 172),   // 10 ms
    T(pCompRelease,   "comp_release", "RELEASE",    Comp, ALL, compTime, 13, 23, 3 + 12, "ms", 176), // 100 ms
    I(pCompGain,      "comp_gain",    "GAIN",       Comp, ALL, -6, 6, 0, "dB", 180, Plain),
    // ------------------------------------------------------------- Filter / Mod
    B(pFltOn,   "flt_on",   "FILTER/MOD", Filter, ALL, 0, 288, BoolInv),
    C(pFltAlgo, "flt_algo", "ALGORITHM",  Filter, ALL, kFilterAlgos, 0, 5, VintagePhaser, 228, Plain),
    I(pFltMix,  "flt_mix",  "MIX",        Filter, b(AutoRes) | b(Resonance) | kPhasers, 0, 100, 50, "%", 280, Plain),
    I(pFltOut,  "flt_out",  "OUT LEVEL",  Filter, ALL, -50, 50, 0, "dB", 284, Plain),
    C(pFltOrder,    "flt_order", "ORDER",       Filter, b(AutoRes) | b(Resonance), kOrder, 0, 1, 0, 232, Plain),
    I(pFltSens,     "flt_sens",  "SENSITIVITY", Filter, b(AutoRes), 0, 10, 6, "", 236, Plain),
    C(pFltResponse, "flt_resp",  "RESPONSE",    Filter, b(AutoRes), kResponse, 0, 2, 1, 240, Plain),
    T(pFltFreqMax,  "flt_fmax",  "FREQ MAX",    Filter, b(AutoRes), hiCut, 34, 54, 44, "Hz", 252),     // 3.16 kHz
    T(pFltResHiCut, "flt_rhicut","HI CUT",      Filter, b(Resonance), hiCut, 18, 57, 36, "Hz", 272),   // 1.26 kHz
    I(pFltResonance,"flt_reso",  "HI RESO",     Filter, b(Resonance), 0, 100, 50, "%", 276, Plain),
    T(pFltSpeed,    "flt_speed", "SPEED",       Filter, kLfoFilt, speed, 0, 208, 64, "Hz", 244),        // ~0.307 Hz
    I(pFltDepth,    "flt_depth", "DEPTH",       Filter, kLfoFilt, 0, 100, 60, "%", 248, Plain),
    T(pFltTempo,    "flt_tempo", "TEMPO",       Filter, kLfoFilt, tapTempo, 0, 16, 0, "", 252),
    I(pFltFeedback, "flt_fb",    "FB",          Filter, kPhasers, -100, 100, 30, "", 260, Plain),
    C(pFltPhRange,  "flt_range", "RANGE",       Filter, kPhasers, kPhRange, 0, 1, 0, 264, Plain),
    B(pFltPhReverse,"flt_phrev", "PHASE REVERSE", Filter, kPhasers, 0, 268, BoolInv),
    C(pFltTremType, "flt_ttype", "TYPE",        Filter, b(Tremolo), kTremType, 0, 1, 0, 256, Plain),
    I(pFltPulseWidth,"flt_pw",   "PULSE WIDTH", Filter, b(Tremolo), 0, 100, 50, "%", 260, Plain),
    T(pFltTremHiCut,"flt_thicut","HI CUT",      Filter, b(Tremolo), hiCut, 0, 60, 60, "Hz", 272),
    // ------------------------------------------------------------- Pitch
    B(pPitOn,   "pit_on",   "PITCH",     Pitch, ALL, 0, 352, BoolInv),
    C(pPitAlgo, "pit_algo", "ALGORITHM", Pitch, ALL, kPitchAlgos, 0, 3, Detune, 292, Plain),
    I(pPitMix,  "pit_mix",  "MIX",       Pitch, b(Detune) | b(Octaver) | b(Shifter), 0, 100, 50, "%", 340, Plain),
    I(pPitOut,  "pit_out",  "OUT LEVEL", Pitch, ALL, -50, 50, 0, "dB", 344, Plain),
    I(pDetVoice1, "det_v1", "VOICE 1", Pitch, b(Detune), -100, 100, 9, "cents", 296, Plain),
    I(pDetVoice2, "det_v2", "VOICE 2", Pitch, b(Detune), -100, 100, -9, "cents", 300, Plain),
    I(pDetDelay1, "det_d1", "DELAY 1", Pitch, b(Detune), 0, 50, 10, "ms", 312, Plain),
    I(pDetDelay2, "det_d2", "DELAY 2", Pitch, b(Detune), 0, 50, 15, "ms", 316, Plain),
    I(pWhamPitch, "wh_pitch","PITCH",  Pitch, b(Whammy), 0, 100, 0, "%", 328, Plain),
    C(pPitDirection,"pit_dir","DIRECTION", Pitch, b(Whammy) | b(Octaver), kDirection, 0, 1, 1, 332, Plain),
    C(pPitRange,  "pit_range","RANGE",  Pitch, b(Whammy) | b(Octaver), kOctRange, 1, 2, 1, 336, Plain),
    I(pShfVoice1, "shf_v1", "VOICE 1", Pitch, b(Shifter), -2400, 2400, -1200, "cents", 296, Plain),
    I(pShfVoice2, "shf_v2", "VOICE 2", Pitch, b(Shifter), -2400, 2400, 700, "cents", 300, Plain),
    I(pShfPan1,   "shf_pan1","PAN 1",  Pitch, b(Shifter), -50, 50, -50, "", 304, Plain),
    I(pShfPan2,   "shf_pan2","PAN 2",  Pitch, b(Shifter), -50, 50, 50, "", 308, Plain),
    I(pShfDelay1, "shf_d1", "DELAY 1", Pitch, b(Shifter), 0, 350, 0, "ms", 312, Plain),
    I(pShfDelay2, "shf_d2", "DELAY 2", Pitch, b(Shifter), 0, 350, 0, "ms", 316, Plain),
    I(pShfFb1,    "shf_fb1","FB 1",    Pitch, b(Shifter), 0, 100, 0, "%", 320, Plain),
    I(pShfFb2,    "shf_fb2","FB 2",    Pitch, b(Shifter), 0, 100, 0, "%", 324, Plain),
    I(pShfLevel1, "shf_l1", "LEVEL 1", Pitch, b(Shifter), -100, 0, 0, "dB", 328, Plain),
    I(pShfLevel2, "shf_l2", "LEVEL 2", Pitch, b(Shifter), -100, 0, -100, "dB", 332, Plain),
    // ------------------------------------------------------------- Chorus / Flanger
    B(pModOn,   "mod_on",   "CHORUS/FLANGE", Mod, ALL, 0, 416, BoolInv),
    C(pModAlgo, "mod_algo", "ALGORITHM", Mod, ALL, kModAlgos, 0, 4, ClassicChorus, 356, Plain),
    I(pModMix,  "mod_mix",  "MIX",       Mod, ALL & ~b(Vibrato), 0, 100, 50, "%", 396, Plain),
    I(pModOut,  "mod_out",  "OUT LEVEL", Mod, ALL, -50, 50, 0, "dB", 400, Plain),
    T(pModSpeed,"mod_speed","SPEED",     Mod, ALL, speed, 0, 208, 80, "Hz", 360),   // .501 Hz
    I(pModDepth,"mod_depth","DEPTH",     Mod, ALL, 0, 100, 40, "%", 364, Plain),
    T(pModTempo,"mod_tempo","TEMPO",     Mod, ALL, tapTempo, 0, 16, 0, "", 368),
    T(pModHiCut,"mod_hicut","HI CUT",    Mod, ALL, hiCut, 0, 60, 60, "Hz", 372),
    I(pModFeedback,"mod_fb", "FB",       Mod, b(ClassicFlange) | b(AdvFlange), -100, 100, 50, "", 376, Plain),
    T(pModFbHiCut,"mod_fbhicut","FB HI CUT", Mod, b(ClassicFlange) | b(AdvFlange), hiCut, 0, 60, 60, "Hz", 380),
    I(pModDelay,"mod_delay","DELAY",     Mod, b(AdvChorus) | b(AdvFlange), 1, 500, 100, "/10 ms", 384, Plain),
    B(pModGoldRatio,"mod_gold","GOLDEN RATIO", Mod, b(AdvChorus) | b(AdvFlange), 1, 388, Plain),
    B(pModPhReverse,"mod_phrev","PHASE REVERSE", Mod, b(AdvChorus) | b(AdvFlange), 0, 392, Plain),
    // ------------------------------------------------------------- Delay
    B(pDlyOn,   "dly_on",   "DELAY",     Delay, ALL, 0, 480, BoolInv),
    C(pDlyAlgo, "dly_algo", "ALGORITHM", Delay, ALL, kDelayAlgos, 0, 2, PingPong, 420, Plain),
    I(pDlyMix,  "dly_mix",  "MIX",       Delay, ALL, 0, 100, 25, "%", 472, Plain),
    I(pDlyOut,  "dly_out",  "OUT LEVEL", Delay, ALL, -50, 50, 0, "dB", 476, Plain),
    I(pDlyTime, "dly_time", "TIME",      Delay, ALL, 0, 1800, 420, "ms", 424, Plain),
    T(pDlyTempo,"dly_tempo","TEMPO",     Delay, ALL, tapTempo, 0, 16, 0, "", 432),
    I(pDlyFeedback,"dly_fb","FB",        Delay, ALL, 0, 100, 35, "%", 440, Plain),
    T(pDlyFbHiCut,"dly_fbhicut","FB HI CUT", Delay, ALL, hiCut, 40, 60, 50, "Hz", 448),   // 6.31 kHz
    T(pDlyFbLoCut,"dly_fblocut","FB LO CUT", Delay, ALL, loCut, 0, 40, 14, "Hz", 452),    // 100 Hz
    I(pDlyWidth,"dly_width","WIDTH",     Delay, b(PingPong), 0, 100, 100, "%", 436, Plain),
    I(pDlyTime2,"dly_time2","TIME 2",    Delay, b(Dual), 0, 1800, 630, "ms", 428, Plain),
    T(pDlyTempo2,"dly_tempo2","TEMPO 2", Delay, b(Dual), tapTempo, 0, 16, 0, "", 436),
    I(pDlyFeedback2,"dly_fb2","FB 2",    Delay, b(Dual), 0, 100, 30, "%", 444, Plain),
    I(pDlyPan1, "dly_pan1", "PAN 1",     Delay, b(Dual), -50, 50, -50, "", 456, Plain),
    I(pDlyPan2, "dly_pan2", "PAN 2",     Delay, b(Dual), -50, 50, 50, "", 460, Plain),
    I(pDlyOffset,"dly_offset","OFFSET",  Delay, b(Dynamic), -200, 200, 20, "ms", 456, Plain),
    I(pDlySense,"dly_sense","SENSIT",    Delay, b(Dynamic), -50, 0, -30, "dB", 460, Plain),
    I(pDlyDamp, "dly_damp", "DAMP",      Delay, b(Dynamic), 0, 100, 12, "dB", 464, Plain),
    T(pDlyRelease,"dly_rel","RELEASE",   Delay, b(Dynamic), compTime, 11, 21, 3 + 12, "ms", 468), // 100 ms
    // ------------------------------------------------------------- Reverb
    B(pRevOn,   "rev_on",   "REVERB",    Reverb, ALL, 0, 544, BoolInv),
    C(pRevAlgo, "rev_algo", "ALGORITHM", Reverb, ALL, kReverbAlgos, 0, 3, Hall, 484, Plain),
    I(pRevMix,  "rev_mix",  "MIX",       Reverb, ALL, 0, 100, 25, "%", 532, Plain),
    I(pRevOut,  "rev_out",  "OUT LEVEL", Reverb, ALL, -50, 50, 0, "dB", 536, Plain),
    I(pRevDecay,"rev_decay","DECAY",     Reverb, ALL, 1, 200, 25, "/10 s", 488, Plain),
    I(pRevPreDelay,"rev_predly","PRE DLY", Reverb, ALL, 0, 100, 10, "ms", 492, Plain),
    C(pRevShape,"rev_shape","SHAPE",     Reverb, ALL, kShape, 0, 2, 1, 496, Plain),
    T(pRevSize, "rev_size", "SIZE",      Reverb, ALL, reverbSize, 0, 7, 3, "", 500),
    T(pRevHiColor,"rev_hicol","HI COL",  Reverb, ALL, reverbHiColor, 0, 6, 2, "", 504),
    I(pRevHiFactor,"rev_hifac","HI FAC", Reverb, ALL, -25, 25, 0, "", 508, Plain),
    T(pRevLoColor,"rev_locol","LO COL",  Reverb, ALL, reverbLoColor, 0, 6, 2, "", 512),
    I(pRevLoFactor,"rev_lofac","LO FAC", Reverb, ALL, -25, 25, 0, "", 516, Plain),
    I(pRevRoomLevel,"rev_roomlvl","ROOM LVL", Reverb, ALL, -100, 0, 0, "dB", 520, Plain),
    I(pRevLevel,"rev_lvl", "REVB LVL",   Reverb, ALL, -100, 0, -3, "dB", 524, Plain),
    I(pRevDiffuse,"rev_diffuse","DIFUSE", Reverb, ALL, -25, 25, 0, "", 528, Plain),
    // ------------------------------------------------------------- Live / unit (not in preset)
    I(pInLevel,    "in_level",   "INPUT",     Live, ALL, -50, 50, 0, "dB", -1, None),
    I(pGlobalOut,  "global_out", "GLOBAL OUT",Live, ALL, -50, 50, 0, "dB", -1, None),
    B(pKillDry,    "kill_dry",   "KILL DRY",  Live, ALL, 0, -1, None),
    B(pBypass,     "bypass",     "BYPASS",    Live, ALL, 0, -1, None),
    C(pTempoSource,"tempo_src",  "TEMPO SRC", Live, ALL, kTempoSrc, 0, 1, 0, -1, None),
    I(pMod1, "mod1", "M1", Live, ALL, 0, 127, 0, "", -1, None),
    I(pMod2, "mod2", "M2", Live, ALL, 0, 127, 0, "", -1, None),
    I(pMod3, "mod3", "M3", Live, ALL, 0, 127, 0, "", -1, None),
    I(pMod4, "mod4", "M4", Live, ALL, 0, 127, 0, "", -1, None),
};
#undef I
#undef T
#undef C
#undef B

// Modifier slot offsets — CONFIRMED (GmFilterModEffect / GmPitchEffect / GmModulationEffect /
// GmDelayEffect / GmReverbEffect constructors: `new ModifireField(offset)`).
const ModSlot kModSlots[] = {
    // Filter block (28..47)
    { pFltMix, Block::Filter, AutoRes, 28 },        { pFltOut, Block::Filter, AutoRes, 32 },
    { pFltResHiCut, Block::Filter, Resonance, 28 }, { pFltResonance, Block::Filter, Resonance, 32 },
    { pFltMix, Block::Filter, Resonance, 36 },      { pFltOut, Block::Filter, Resonance, 40 },
    { pFltSpeed, Block::Filter, VintagePhaser, 28 },{ pFltDepth, Block::Filter, VintagePhaser, 32 },
    { pFltFeedback, Block::Filter, VintagePhaser, 36 }, { pFltMix, Block::Filter, VintagePhaser, 40 },
    { pFltOut, Block::Filter, VintagePhaser, 44 },
    { pFltSpeed, Block::Filter, SmoothPhaser, 28 }, { pFltDepth, Block::Filter, SmoothPhaser, 32 },
    { pFltFeedback, Block::Filter, SmoothPhaser, 36 }, { pFltMix, Block::Filter, SmoothPhaser, 40 },
    { pFltOut, Block::Filter, SmoothPhaser, 44 },
    { pFltSpeed, Block::Filter, Tremolo, 28 },      { pFltDepth, Block::Filter, Tremolo, 32 },
    { pFltTremHiCut, Block::Filter, Tremolo, 36 },  { pFltOut, Block::Filter, Tremolo, 40 },
    { pFltSpeed, Block::Filter, Panner, 28 },       { pFltDepth, Block::Filter, Panner, 32 },
    { pFltOut, Block::Filter, Panner, 36 },
    // Pitch block (48..79)
    { pPitMix, Block::Pitch, Detune, 48 },  { pPitOut, Block::Pitch, Detune, 52 },
    { pWhamPitch, Block::Pitch, Whammy, 48 }, { pPitOut, Block::Pitch, Whammy, 52 },
    { pPitMix, Block::Pitch, Octaver, 48 }, { pPitOut, Block::Pitch, Octaver, 52 },
    { pShfVoice1, Block::Pitch, Shifter, 48 }, { pShfVoice2, Block::Pitch, Shifter, 52 },
    { pShfPan1, Block::Pitch, Shifter, 56 },   { pShfPan2, Block::Pitch, Shifter, 60 },
    { pShfFb1, Block::Pitch, Shifter, 64 },    { pShfFb2, Block::Pitch, Shifter, 68 },
    { pPitMix, Block::Pitch, Shifter, 72 },    { pPitOut, Block::Pitch, Shifter, 76 },
    // Chorus/Flanger block (80..107) — speed/depth/hicut common to all algorithms
    { pModSpeed, Block::Mod, ClassicChorus, 80 }, { pModDepth, Block::Mod, ClassicChorus, 84 }, { pModHiCut, Block::Mod, ClassicChorus, 88 },
    { pModMix, Block::Mod, ClassicChorus, 92 },   { pModOut, Block::Mod, ClassicChorus, 96 },
    { pModSpeed, Block::Mod, AdvChorus, 80 },     { pModDepth, Block::Mod, AdvChorus, 84 },     { pModHiCut, Block::Mod, AdvChorus, 88 },
    { pModMix, Block::Mod, AdvChorus, 92 },       { pModOut, Block::Mod, AdvChorus, 96 },
    { pModSpeed, Block::Mod, ClassicFlange, 80 }, { pModDepth, Block::Mod, ClassicFlange, 84 }, { pModHiCut, Block::Mod, ClassicFlange, 88 },
    { pModFeedback, Block::Mod, ClassicFlange, 92 }, { pModFbHiCut, Block::Mod, ClassicFlange, 96 },
    { pModMix, Block::Mod, ClassicFlange, 100 },  { pModOut, Block::Mod, ClassicFlange, 104 },
    { pModSpeed, Block::Mod, AdvFlange, 80 },     { pModDepth, Block::Mod, AdvFlange, 84 },     { pModHiCut, Block::Mod, AdvFlange, 88 },
    { pModFeedback, Block::Mod, AdvFlange, 92 },  { pModFbHiCut, Block::Mod, AdvFlange, 96 },
    { pModMix, Block::Mod, AdvFlange, 100 },      { pModOut, Block::Mod, AdvFlange, 104 },
    { pModSpeed, Block::Mod, Vibrato, 80 },       { pModDepth, Block::Mod, Vibrato, 84 },       { pModHiCut, Block::Mod, Vibrato, 88 },
    { pModOut, Block::Mod, Vibrato, 92 },
    // Delay block (108..147)
    { pDlyTime, Block::Delay, PingPong, 108 }, { pDlyFeedback, Block::Delay, PingPong, 112 },
    { pDlyFbHiCut, Block::Delay, PingPong, 116 }, { pDlyFbLoCut, Block::Delay, PingPong, 120 },
    { pDlyMix, Block::Delay, PingPong, 124 },  { pDlyOut, Block::Delay, PingPong, 128 },
    { pDlyTime, Block::Delay, Dynamic, 108 },  { pDlyFeedback, Block::Delay, Dynamic, 112 },
    { pDlyFbHiCut, Block::Delay, Dynamic, 116 }, { pDlyFbLoCut, Block::Delay, Dynamic, 120 },
    { pDlyMix, Block::Delay, Dynamic, 124 },   { pDlyOut, Block::Delay, Dynamic, 128 },
    { pDlyTime, Block::Delay, Dual, 108 },     { pDlyTime2, Block::Delay, Dual, 112 },
    { pDlyFeedback, Block::Delay, Dual, 116 }, { pDlyFeedback2, Block::Delay, Dual, 120 },
    { pDlyFbHiCut, Block::Delay, Dual, 124 },  { pDlyFbLoCut, Block::Delay, Dual, 128 },
    { pDlyPan1, Block::Delay, Dual, 132 },     { pDlyPan2, Block::Delay, Dual, 136 },
    { pDlyMix, Block::Delay, Dual, 140 },      { pDlyOut, Block::Delay, Dual, 144 },
    // Reverb block (148..155) — all algorithms
    { pRevMix, Block::Reverb, Spring, 148 }, { pRevOut, Block::Reverb, Spring, 152 },
    { pRevMix, Block::Reverb, Hall, 148 },   { pRevOut, Block::Reverb, Hall, 152 },
    { pRevMix, Block::Reverb, Room, 148 },   { pRevOut, Block::Reverb, Room, 152 },
    { pRevMix, Block::Reverb, Plate, 148 },  { pRevOut, Block::Reverb, Plate, 152 },
};

} // namespace

const ParamDef& paramDef(int index) noexcept { return kParams[std::clamp(index, 0, (int)kNumParams - 1)]; }

int paramIndexForKey(const char* key) noexcept {
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp(kParams[i].key, key) == 0) return i;
    return -1;
}

const char* blockName(Block bl) noexcept {
    switch (bl) {
        case Block::Global: return "PATCH";        case Block::Gate: return "NOISE GATE";
        case Block::Eq: return "EQUALIZER";        case Block::Comp: return "COMPRESSOR";
        case Block::Filter: return "FILTER/MOD";   case Block::Pitch: return "PITCH";
        case Block::Mod: return "CHORUS/FLANGE";   case Block::Delay: return "DELAY";
        case Block::Reverb: return "REVERB";       case Block::Live: return "LIVE";
        default: return "?";
    }
}

int algoParamFor(Block bl) noexcept {
    switch (bl) {
        case Block::Filter: return pFltAlgo; case Block::Pitch: return pPitAlgo;
        case Block::Mod: return pModAlgo;    case Block::Delay: return pDlyAlgo;
        case Block::Reverb: return pRevAlgo; default: return -1;
    }
}

int numAlgorithms(Block bl) noexcept {
    const int a = algoParamFor(bl);
    return a < 0 ? 1 : kParams[a].hi + 1;
}

const char* algorithmName(Block bl, int a) noexcept {
    const int p = algoParamFor(bl);
    if (p < 0) return blockName(bl);
    return kParams[p].choices[std::clamp(a, 0, kParams[p].hi)];
}

bool paramUsedBy(const ParamDef& d, int a) noexcept {
    return a < 0 || a > 31 || (d.algoMask & (1u << a)) != 0;
}

float physical(int index, int stored) noexcept {
    const ParamDef& d = paramDef(index);
    stored = std::clamp(stored, d.lo, d.hi);
    if (d.kind == Kind::Table) return tableValue(*d.table, stored);
    return (float)stored;
}

void formatValue(int index, int stored, char* out, int n) noexcept {
    const ParamDef& d = paramDef(index);
    stored = std::clamp(stored, d.lo, d.hi);
    switch (d.kind) {
        case Kind::Bool:
        case Kind::Choice: std::snprintf(out, (size_t)n, "%s", d.choices[stored - d.lo]); return;
        case Kind::Table: {
            const char* lab = tableLabel(*d.table, stored);
            const bool special = tableValue(*d.table, stored) < 0.0f || d.table == &tables::tapTempo || d.table == &tables::compRatio;
            if (special || !d.unit[0]) std::snprintf(out, (size_t)n, "%s", lab);
            else std::snprintf(out, (size_t)n, "%s %s", lab, d.unit);
            return;
        }
        case Kind::Int:
            if (index == pShfPan1 || index == pShfPan2 || index == pDlyPan1 || index == pDlyPan2) {
                if (stored == 0) std::snprintf(out, (size_t)n, "C");
                else std::snprintf(out, (size_t)n, "%d%s", std::abs(stored), stored < 0 ? "L" : "R");
                return;
            }
            if (d.unit[0]) std::snprintf(out, (size_t)n, "%d %s", stored, d.unit);
            else std::snprintf(out, (size_t)n, "%d", stored);
            return;
    }
}

bool isExtendedLevel(int p) noexcept {
    return p == pOutLevel || p == pFltOut || p == pPitOut || p == pModOut || p == pDlyOut || p == pRevOut;
}

void hardwareRange(int p, int& lo, int& hi) noexcept {
    if (isExtendedLevel(p)) { lo = -100; hi = 0; return; }   // G-Major limits (-100 = Off)
    lo = paramDef(p).lo; hi = paramDef(p).hi;
}

int modifierOffset(int param, int a) noexcept {
    for (const auto& s : kModSlots)
        if (s.param == param && s.algo == a) return s.offset;
    // Blocks without algorithm choice never have modifiers in this table
    return -1;
}

const ModSlot* modSlots(int& count) noexcept {
    count = (int)(sizeof(kModSlots) / sizeof(kModSlots[0]));
    return kModSlots;
}

} // namespace gmr
