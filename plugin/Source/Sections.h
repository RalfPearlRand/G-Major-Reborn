#pragma once
#include "Controls.h"

namespace ui {

/** One G-Major effect block: header (on/off LED, title, Undo, Edit-from-library) + controls. */
class EffectSection : public juce::Component {
public:
    EffectSection(GMajorProcessor& p, gmr::Block b);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();                         // algorithm-dependent visibility + modifier buttons
    int preferredHeight() const;
    const gmr::Block block;

protected:
    void snapshot();                        // push undo state (before an edit)
    void undo();
    void showEditMenu();
    std::vector<int> blockParams() const;   // every preset parameter of this block
    virtual void layoutControls(juce::Rectangle<int> area);

    GMajorProcessor& proc;
    std::unique_ptr<LedGroup> onLed;
    std::unique_ptr<AlgoSelector> algo;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<LedGroup> groups;
    Knob* mixKnob = nullptr;
    Knob* outKnob = nullptr;
    juce::TextButton undoBtn { "U" }, editBtn { "E" };
    struct Snap { std::vector<int> values; std::vector<gmr::Modifier> mods; };
    std::vector<Snap> history;
    static constexpr int kHeader = 20;
};

/** EQ: three columns BAND 1-3 x rows FREQ / WIDTH / GAIN (GLydian layout). */
class EqSection : public EffectSection {
public:
    explicit EqSection(GMajorProcessor& p) : EffectSection(p, gmr::Block::Eq) {}
protected:
    void layoutControls(juce::Rectangle<int> area) override;
    void paintOverChildren(juce::Graphics&) override;
};

/** Top-left panel: transfer buttons, bank/name/number, routing, out level, tap tempo, relays. */
class PatchPanel : public juce::Component, private juce::Timer {
public:
    explicit PatchPanel(GMajorProcessor& p);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    std::function<void()> onOpenDisk, onSaveDisk, onGetLib;
private:
    void timerCallback() override;
    GMajorProcessor& proc;
    juce::TextButton txPlaying { "Tx Playing" }, txBank { "Tx Bank" }, saveDisk { "Save Disk" }, addLib { "Add Lib" },
                     rxPlaying { "Rx Playing" }, rxBank { "Rx Bank" }, openDisk { "Open Disk" }, getLib { "Get Lib" };
    juce::ToggleButton factory { "FACTORY" }, user { "USER" }, txRealtime { "Tx RealTime" };
    juce::TextEditor name;
    juce::Slider number;
    LedGroup routing;
    Knob outLevel, tapTempo;
    juce::TextButton tap { "TAP" };
    LedGroup relay1, relay2;
    juce::Array<double> taps;
};

/** Bottom strip: unit-level controls that the hardware keeps outside the preset. */
class LiveStrip : public juce::Component, private juce::Timer {
public:
    explicit LiveStrip(GMajorProcessor& p);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    GMajorProcessor& proc;
    Knob input, globalOut;
    LedGroup killDry, bypass, tempoSrc;
    juce::OwnedArray<juce::Slider> mods;
    juce::OwnedArray<juce::SliderParameterAttachment> modAtt;
    juce::OwnedArray<juce::Label> modLabels;
    Meter inMeter { "IN" }, outMeter { "OUT" }, gateMeter { "GATE" }, compMeter { "COMP" };
    juce::Label status;
};

} // namespace ui
