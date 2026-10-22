#pragma once
#include "Sections.h"

/** Editor of the single-block "G-Major <Block> Rack" plugins (v0.9.1). */
class RackEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener {
public:
    explicit RackEditor(GMajorProcessor&);
    ~RackEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { rebuildPrograms(); }
    void rebuildPrograms();
    void openSyx();
    void tap();

    GMajorProcessor& proc;
    ui::LookAndFeel lnf;
    std::unique_ptr<ui::EffectSection> section;
    juce::ComboBox programs;
    juce::TextButton openBtn { "Open .syx" };
    ui::Knob input, output;
    ui::LedGroup bypass;
    std::unique_ptr<ui::LedGroup> killDry, tempoSrc;
    std::unique_ptr<ui::Knob> tapTempo;
    juce::TextButton tapBtn { "TAP" };
    juce::OwnedArray<juce::Slider> mods;
    juce::OwnedArray<juce::SliderParameterAttachment> modAtt;
    juce::OwnedArray<juce::Label> modLabels;
    ui::Meter inMeter { "IN" }, outMeter { "OUT" };
    std::unique_ptr<ui::Meter> dynMeter;
    juce::Array<double> taps;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
    static constexpr int kTop = 30, kStrip = 86;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackEditor)
};
