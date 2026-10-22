// G-Major Reborn — value tables recovered from the GLydian editor
// (com.glydian.model.DiscretRange, static String[] initialisers).
//
// Storage rule (CONFIRMED, DiscretRange.getItem / getIndexForValue):
//     storedValue = tableIndex + table.storeOffset
// storeOffset is 3 for COMP_TIME and EQ_WIDTH, 25 for EQ_FREQ, 0 otherwise.
#pragma once
#include <cstddef>

namespace gmr {

struct ValueTable {
    const char* const* labels;   // display strings (trimmed)
    const float* values;         // numeric value in base unit (Hz, ms, ratio, octaves); <0 = special
    int size;
    int storeOffset;
};

namespace tables {
extern const ValueTable compTime;    // ms        (attack / release / delay release)
extern const ValueTable compRatio;   // ratio     (0 = Off, 1e9 = Inf)
extern const ValueTable tapTempo;    // note frac (0 = Ignored), value = fraction of a whole note
extern const ValueTable speed;       // Hz
extern const ValueTable hiCut;       // Hz        (last entry "Off" = -1)
extern const ValueTable loCut;       // Hz        (first entry "Off" = -1)
extern const ValueTable eqWidth;     // octaves
extern const ValueTable eqFreq;      // Hz        (last entry "Off" = -1)
extern const ValueTable reverbSize;  // index
extern const ValueTable reverbHiColor;
extern const ValueTable reverbLoColor;
} // namespace tables

/** Returns numeric value for a stored (SysEx) value, clamped to the table. */
float tableValue(const ValueTable& t, int stored) noexcept;
const char* tableLabel(const ValueTable& t, int stored) noexcept;
/** Absolute stored value for a label (exact match, trimmed). -1 if not found. */
int tableStoredForLabel(const ValueTable& t, const char* label) noexcept;

} // namespace gmr
