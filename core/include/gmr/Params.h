// G-Major Reborn — single source of truth for every parameter.
//
// Every value is kept in the *stored* (SysEx) domain of the G-Major, i.e. the same
// integer the hardware keeps in its 615-byte preset. Offsets and ranges were
// recovered from the GLydian editor (com.glydian.model.effect.*). See
// docs/SYSEX_FORMAT.md for the derivation and confidence level of each field.
#pragma once
#include "gmr/Tables.h"
#include <cstdint>

namespace gmr {

enum class Block : uint8_t { Global, Gate, Eq, Comp, Filter, Pitch, Mod, Delay, Reverb, Live, Count };

namespace algo {
enum Filter : int { AutoRes = 0, Resonance = 1, VintagePhaser = 2, SmoothPhaser = 3, Tremolo = 4, Panner = 5, FilterCount = 6 };
enum Pitch  : int { Detune = 0, Whammy = 1, Octaver = 2, Shifter = 3, PitchCount = 4 };
enum Mod    : int { ClassicChorus = 0, AdvChorus = 1, ClassicFlange = 2, AdvFlange = 3, Vibrato = 4, ModCount = 5 };
enum Delay  : int { PingPong = 0, Dynamic = 1, Dual = 2, DelayCount = 3 };
enum Reverb : int { Spring = 0, Hall = 1, Room = 2, Plate = 3, ReverbCount = 4 };
}

// ---------------------------------------------------------------------------
// Parameter indices. ORDER MATTERS: must match kParams[] in Params.cpp.
// ---------------------------------------------------------------------------
enum P : int {
    // --- Global (stored in patch) ---
    pRouting, pOutLevel, pRelay1, pRelay2, pTapTempo,
    // --- Noise gate ---
    pGateOn, pGateMode, pGateThreshold, pGateMaxDamp, pGateRelease,
    // --- EQ ---
    pEqOn, pEq1Freq, pEq1Gain, pEq1Width, pEq2Freq, pEq2Gain, pEq2Width, pEq3Freq, pEq3Gain, pEq3Width,
    // --- Compressor ---
    pCompOn, pCompThreshold, pCompRatio, pCompAttack, pCompRelease, pCompGain,
    // --- Filter / Mod ---
    pFltOn, pFltAlgo, pFltMix, pFltOut, pFltOrder, pFltSens, pFltResponse, pFltFreqMax,
    pFltResHiCut, pFltResonance, pFltSpeed, pFltDepth, pFltTempo, pFltFeedback, pFltPhRange,
    pFltPhReverse, pFltTremType, pFltPulseWidth, pFltTremHiCut,
    // --- Pitch ---
    pPitOn, pPitAlgo, pPitMix, pPitOut,
    pDetVoice1, pDetVoice2, pDetDelay1, pDetDelay2,
    pWhamPitch, pPitDirection, pPitRange,
    pShfVoice1, pShfVoice2, pShfPan1, pShfPan2, pShfDelay1, pShfDelay2, pShfFb1, pShfFb2, pShfLevel1, pShfLevel2,
    // --- Chorus / Flanger ---
    pModOn, pModAlgo, pModMix, pModOut, pModSpeed, pModDepth, pModTempo, pModHiCut,
    pModFeedback, pModFbHiCut, pModDelay, pModGoldRatio, pModPhReverse,
    // --- Delay ---
    pDlyOn, pDlyAlgo, pDlyMix, pDlyOut, pDlyTime, pDlyTempo, pDlyFeedback, pDlyFbHiCut, pDlyFbLoCut,
    pDlyWidth, pDlyTime2, pDlyTempo2, pDlyFeedback2, pDlyPan1, pDlyPan2,
    pDlyOffset, pDlySense, pDlyDamp, pDlyRelease,
    // --- Reverb ---
    pRevOn, pRevAlgo, pRevMix, pRevOut, pRevDecay, pRevPreDelay, pRevShape, pRevSize,
    pRevHiColor, pRevHiFactor, pRevLoColor, pRevLoFactor, pRevRoomLevel, pRevLevel, pRevDiffuse,
    // --- Live / global unit settings (NOT stored in the G-Major preset, like the hardware) ---
    pInLevel, pGlobalOut, pKillDry, pBypass, pTempoSource, pMod1, pMod2, pMod3, pMod4,
    kNumParams
};

enum class Kind : uint8_t { Int, Table, Choice, Bool };
enum class Enc  : uint8_t {
    Plain,      // 4-byte packed signed int at `offset`
    BoolInv,    // 4-byte packed, 0 = ON, 1 = OFF   (block on/off flags, phaser phase reverse)
    Special,    // handled explicitly by the codec (routing/out level/relays bit field)
    None        // not part of the preset
};

struct ParamDef {
    P id;
    const char* key;          // stable automation / state ID
    const char* label;        // short panel label (GLydian wording)
    Block block;
    uint32_t algoMask;        // bit n set => used by algorithm n of the block (0xFFFFFFFF = all)
    Kind kind;
    int lo, hi, def;          // stored-domain range & default
    const ValueTable* table;  // Kind::Table
    const char* const* choices; // Kind::Choice (hi-lo+1 entries)
    const char* unit;
    int offset;               // byte offset inside SysexMessage.getData() (no leading F0)
    Enc enc;
};

const ParamDef& paramDef(int index) noexcept;
int paramIndexForKey(const char* key) noexcept; // -1 if unknown
const char* blockName(Block b) noexcept;
/** Algorithm selector parameter for a block, or -1 when the block has a single algorithm. */
int algoParamFor(Block b) noexcept;
int numAlgorithms(Block b) noexcept;
const char* algorithmName(Block b, int algo) noexcept;
/** True when the parameter is relevant for the given algorithm of its block. */
bool paramUsedBy(const ParamDef& d, int algo) noexcept;
/** Human readable value, e.g. "2.00k Hz", "-12 dB", "1/8T". */
void formatValue(int index, int stored, char* out, int outSize) noexcept;
/** Numeric value in natural units (Hz, ms, dB, %, ratio...) for a stored value. */
float physical(int index, int stored) noexcept;

/** Level controls whose range was extended to -50..+50 dB (user request v0.9.1). The hardware
 *  only stores -100..0 dB for them; see hardwareRange() and the extension SysEx in Patch.h. */
bool isExtendedLevel(int p) noexcept;
/** Range the real G-Major accepts for a parameter (differs from ParamDef only for extended levels). */
void hardwareRange(int p, int& lo, int& hi) noexcept;

// ---------------------------------------------------------------------------
// Modifier slots (M1..M4 response curves) — CONFIRMED offsets per algorithm.
// ---------------------------------------------------------------------------
struct ModSlot { P param; Block block; int algo; int offset; };
/** Offset of the 4-byte modifier slot for `param` under `algo`, or -1 if not assignable. */
int modifierOffset(int param, int algo) noexcept;
const ModSlot* modSlots(int& count) noexcept;

} // namespace gmr
