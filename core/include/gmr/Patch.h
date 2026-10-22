// G-Major Reborn — preset model + G-Major SysEx codec.
//
// Message layout (offsets are into SysexMessage.getData(), i.e. WITHOUT the leading F0;
// add 1 for offsets into the raw .syx file):
//   [0..4]   00 20 1F 00 48   TC Electronic manufacturer ID, device 00, model 0x48 (G-Major)
//   [5]      0x20             "preset data" message type
//   [6],[7]  lsb, msb         absolute preset number = lsb + 128*msb
//                               0 = edit buffer, 1..100 factory, 101..200 user   (inferred)
//   [8..27]  name, 20 ASCII chars, space padded
//   [28..155] 32 modifier slots, 4 bytes each (28-bit field, 7 bits per byte)
//   [156..159] bit field: routing(2) | outLevel+100(7) | relay1 | relay2
//   [160..611] parameters, 4 bytes each: 24-bit two's complement packed 7 bits/byte, LSB first
//   [612]    checksum = (sum of bytes [28..611]) & 0x7F
//   [613]    F7
#pragma once
#include "gmr/Params.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gmr {

constexpr int kSysexSize = 615;        // full message including F0 ... F7
constexpr int kNumModSlots = 32;       // offsets 28..155
constexpr int kFirstModOffset = 28;

struct Modifier {
    uint8_t source = 0;   // 0 = OFF, 1..4 = M1..M4
    uint8_t minOut = 0;   // 0..100 %
    uint8_t midOut = 50;
    uint8_t maxOut = 100;
    bool operator==(const Modifier& o) const noexcept {
        return source == o.source && minOut == o.minOut && midOut == o.midOut && maxOut == o.maxOut;
    }
};

enum class Bank : uint8_t { EditBuffer, Factory, User };

struct Patch {
    std::array<int, kNumParams> v {};
    std::array<Modifier, kNumModSlots> mods {};
    std::string name = "G-Major Reborn";
    int number = 0;                                    // absolute preset number (see above)
    std::array<uint8_t, kSysexSize> raw {};            // image of the last decoded message
    bool hasRaw = false;

    Patch();                                           // all parameters at their defaults
    int get(int p) const noexcept { return v[(size_t)p]; }
    void set(int p, int stored) noexcept;              // clamps to the parameter range
    int algo(Block b) const noexcept { int a = algoParamFor(b); return a < 0 ? 0 : v[(size_t)a]; }
    /** Modifier assigned to `param` under the block's current algorithm, or nullptr. */
    const Modifier* modifierFor(int param) const noexcept;
    Modifier* modifierFor(int param) noexcept;

    Bank bank() const noexcept;
    int numberInBank() const noexcept;                 // 1..100 (0 for edit buffer)
    static int absoluteNumber(Bank b, int n) noexcept;
};

// ---- low level field helpers (exposed for tests) --------------------------------------
int32_t readPacked24(const uint8_t* p) noexcept;
void writePacked24(uint8_t* p, int32_t value) noexcept;
uint8_t computeChecksum(const uint8_t* data) noexcept;  // `data` = message without F0

/** GLydian's own (buggy for values <= -128) integer reader, kept for comparison tests. */
int32_t glydianReadInt(const uint8_t* p) noexcept;

struct DecodeResult {
    bool ok = false;
    bool checksumOk = false;
    std::string error;
};

/** Decode a 615-byte G-Major preset message (F0..F7). Unknown bytes are kept in patch.raw. */
DecodeResult decodeSysex(const uint8_t* msg, size_t len, Patch& patch);

/** Encode to a 615-byte message. asEditBuffer => preset number 0 (what GLydian "Tx Playing" does). */
void encodeSysex(const Patch& patch, uint8_t* out615, bool asEditBuffer = false);

// ---- extended levels (v0.9.1) -----------------------------------------------------------
// OUT LEVEL controls go from -50 to +50 dB in the plugin, the hardware stores -100..0 dB. The main
// 615-byte message always carries hardware-valid (clamped) values; when a level is outside that range
// a second message  F0 7D 'G' 'M' 'R' 01 n {off_lo off_hi v0 v1 v2 v3}*n F7  follows it in files.
// 0x7D is the MIDI "non-commercial" manufacturer ID: the G-Major (and any other device) ignores it.
std::vector<uint8_t> encodeExtension(const Patch& p);           // empty when not needed
size_t extensionLengthAt(const uint8_t* data, size_t len, size_t pos) noexcept;   // 0 if none
bool applyExtension(const uint8_t* msg, size_t len, Patch& p) noexcept;
/** 615-byte preset + optional extension, as written to .syx files and plugin state. */
std::vector<uint8_t> encodeForFile(const Patch& p);
/** Finds the next preset at/after `pos`, decodes it (+ extension) and advances `pos` past both. */
bool readPresetAt(const uint8_t* data, size_t len, size_t& pos, Patch& p, DecodeResult* result = nullptr);

/** "F0 00 20 1F 00 48 45 lsb msb F7" — request one preset (0 = current edit buffer). */
std::array<uint8_t, 10> makePresetRequest(int absoluteNumber) noexcept;

/** Search a byte stream (e.g. a .syx file with several presets) for complete G-Major presets. */
size_t findNextPreset(const uint8_t* data, size_t len, size_t start) noexcept; // returns len if none

} // namespace gmr
