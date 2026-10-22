// G-Major Reborn — bundled demo presets (original settings written for this project;
// they are NOT copies of TC Electronic's factory presets).
#pragma once
#include "gmr/Patch.h"
#include <vector>

namespace gmr {
std::vector<Patch> factoryPresets();
/** A neutral starting point: every block off, 0 dB, serial routing. */
Patch initPatch();
/** Number of table labels used by factoryPresets() that do not exist (must be 0). */
int factoryPresetLabelErrors();
}
