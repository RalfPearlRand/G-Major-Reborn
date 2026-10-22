#pragma once
#include <JuceHeader.h>
#include "gmr/Engine.h"
#include "gmr/FactoryPresets.h"
#include "PresetLibrary.h"

/** Lock-free single-producer/single-consumer queue of small MIDI/SysEx messages. */
class MessageFifo {
public:
    static constexpr int kSlots = 64, kMaxBytes = gmr::kSysexSize + 8;
    bool push(const uint8_t* data, int size) noexcept {
        if (size <= 0 || size > kMaxBytes) return false;
        int s1, n1, s2, n2;
        fifo.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 + n2 < 1) return false;
        const int i = n1 > 0 ? s1 : s2;
        std::memcpy(slots[(size_t)i].data(), data, (size_t)size);
        sizes[(size_t)i] = size;
        fifo.finishedWrite(1);
        return true;
    }
    template <typename Fn> void popAll(Fn&& fn) noexcept {
        int s1, n1, s2, n2;
        fifo.prepareToRead(fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) fn(slots[(size_t)(s1 + i)].data(), sizes[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; ++i) fn(slots[(size_t)(s2 + i)].data(), sizes[(size_t)(s2 + i)]);
        fifo.finishedRead(n1 + n2);
    }
private:
    juce::AbstractFifo fifo { kSlots };
    std::array<std::array<uint8_t, kMaxBytes>, kSlots> slots {};
    std::array<int, kSlots> sizes {};
};

class GMajorProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::Timer {
public:
    /** rack = Block::Global: full G-Major. Any effect block: single-block "Rack" plugin (v0.9.1). */
    explicit GMajorProcessor(gmr::Block rack = gmr::Block::Global);
    ~GMajorProcessor() override;
    const gmr::Block rack;
    bool isRack() const noexcept { return rack != gmr::Block::Global; }
    bool hasParam(int p) const noexcept { return raw[(size_t)p] != nullptr; }
    /** Parameters exposed by a rack (block params + the unit controls that block needs). */
    static bool paramInRack(gmr::Block rack, int p) noexcept;

    // ---- AudioProcessor ----
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;     // keep the double-precision overload visible
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- model access (message thread) ----
    juce::AudioProcessorValueTreeState apvts;
    PresetLibrary library;
    gmr::Engine& engine() noexcept { return eng; }

    /** Current preset meta data + raw image (parameters live in the APVTS). */
    gmr::Patch currentPatch() const;
    void loadPatch(const gmr::Patch& p, bool notifyHost = true);
    void setPatchName(const juce::String& n);
    juce::String getPatchName() const { return patchName; }
    void setPatchNumber(int absoluteNumber);
    int getPatchNumber() const { return patchNumber; }

    void setParamStored(int p, int stored);       // sets the APVTS parameter (notifies host)
    int getParamStored(int p) const;

    gmr::Modifier getModifier(int slot) const { return eng.getModifier(slot); }
    void setModifier(int slot, const gmr::Modifier& m);

    // ---- hardware communication (G-Major over MIDI) ----
    // Button semantics CONFIRMED from GLydian (GLydianSwing.MainDisplay listeners):
    void txPlaying();                              // send current patch to the unit's edit buffer
    void txBank();                                 // store current patch at the selected bank/number (FACTORY -> edit buffer)
    void rxPlaying();                              // request the unit's edit buffer
    void rxBank();                                 // request the preset at the selected bank/number and load it
    // Library transfers (GLydian "Patch Library" menu):
    void txLibrary();                              // send every USER patch of the library to its number (paced)
    void rxAllUser();                              // request user presets 1..100 into the library (paced)
    void setTxRealtime(bool on) { txRealtime = on; }
    bool getTxRealtime() const { return txRealtime; }
    juce::String lastMidiStatus() const { const juce::ScopedLock sl(statusLock); return midiStatus; }

    // ---- MIDI settings ----
    std::array<std::atomic<int>, 4> modCC {};      // CC number driving M1..M4 (-1 = none)
    std::atomic<int> learnTarget { -1 };           // 0..3 while learning
    std::atomic<int> midiChannel { 0 };            // 0 = omni, 1..16
    std::atomic<bool> programChangeEnabled { true };
    std::atomic<bool> relayCcOut { false };        // emit CC 80/81 when relays change
    std::function<void()> onPatchChanged;           // UI refresh hook (message thread)

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout(gmr::Block rack = gmr::Block::Global);
    static juce::String rackTitle(gmr::Block b);
    static juce::String paramId(int p) { return gmr::paramDef(p).key; }

private:
    void parameterChanged(const juce::String& id, float newValue) override;
    void timerCallback() override;
    void handleIncomingSysex(const uint8_t* d, int size);
    void queueOut(const uint8_t* d, int size);
    void setStatus(const juce::String& s) { const juce::ScopedLock sl(statusLock); midiStatus = s; }

    gmr::Engine eng;
    std::array<std::atomic<float>*, gmr::kNumParams> raw {};
    int fixedValue(int p) const noexcept;   // value used for parameters a rack does not expose
    juce::String patchName { "Init" };
    int patchNumber = 0;
    std::array<uint8_t, gmr::kSysexSize> rawImage {};
    bool hasRawImage = false;
    int currentProgram = 0;

    MessageFifo midiIn, midiOut;
    std::array<std::atomic<int>, 4> pendingModValue;   // -1 = none, else 0..127 from CC
    std::atomic<int> pendingProgram { -1 };
    std::atomic<bool> dirtyForRealtime { false };
    bool txRealtime = false;
    int realtimeHoldoff = 0;
    int lastRelay[2] { -1, -1 };

    struct OutJob { std::vector<uint8_t> bytes; };
    std::deque<OutJob> outJobs;                    // message thread only, paced by the timer
    int outPace = 0;
    int rxBankNext = -1;
    std::atomic<int> expectLoadNumber { -1 };      // preset number whose dump should become the current patch
    juce::CriticalSection statusLock;
    juce::String midiStatus { "MIDI idle" };
    juce::MidiBuffer scratchMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GMajorProcessor)
};
