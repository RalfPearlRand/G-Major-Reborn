#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui {

namespace colours {
const juce::Colour window   { 0xffe9e9ee };
const juce::Colour panel    { 0xffe2e3ea };
const juce::Colour panelHi  { 0xfff7f7fa };
const juce::Colour blueBox  { 0xffc8d3ee };
const juce::Colour header   { 0xff2c2862 };
const juce::Colour arc      { 0xff34c3e6 };
const juce::Colour text     { 0xff26262e };
const juce::Colour dim      { 0xff6a6a76 };
const juce::Colour led      { 0xff1d1d28 };
}

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(11.5f, juce::Font::plain); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(11.0f, juce::Font::plain); }
    juce::Font getPopupMenuFont() override { return juce::Font(13.0f); }
};

/** Helper owning a ParameterAttachment to one G-Major parameter (stored-domain integers). */
class ParamBinding {
public:
    ParamBinding(GMajorProcessor& p, int index, std::function<void(int)> onChange);
    void set(int stored);                // as a complete undoable gesture
    int get() const;
    juce::RangedAudioParameter& param() { return *prm; }
    const int index;
private:
    GMajorProcessor& proc;
    juce::RangedAudioParameter* prm;
    std::unique_ptr<juce::ParameterAttachment> att;
};

/** Knob + caption + value read-out, optional modifier (M1..M4) button underneath. */
class Knob : public juce::Component {
public:
    Knob(GMajorProcessor& p, int param, std::function<void()> beforeChange = {});
    void setCaption(const juce::String& c) { caption = c; repaint(); }
    void setModifierVisible(bool v);
    bool modifierVisible() const { return modVisible; }
    void refreshModifier();
    /** Compact layout for MIX / OUT LEVEL: small knob with the value on its right (GLydian style). */
    void setCompact(bool c) { compact = c; resized(); repaint(); }
    void resized() override;
    void paint(juce::Graphics&) override;
    static constexpr int kWidth = 50, kHeight = 86, kHeightNoMod = 66, kCompactWidth = 72, kCompactHeight = 62;
    const int param;
private:
    void showModMenu();
    GMajorProcessor& proc;
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    juce::TextButton modBtn { "OFF" };
    juce::String caption, valueText;
    bool modVisible = false, compact = false;
};

/** LED-style radio group (or single toggle for Bool) bound to a Choice/Bool parameter. */
class LedGroup : public juce::Component {
public:
    LedGroup(GMajorProcessor& p, int param, std::function<void()> beforeChange = {}, bool showTitle = true);
    void resized() override;
    void paint(juce::Graphics&) override;
    int preferredHeight() const { return (showTitle ? 14 : 0) + 18 * juce::jmax(1, (int)leds.size()); }
    int preferredWidth() const;
    void setTextColour(juce::Colour c, bool bold);
    const int param;
private:
    ParamBinding binding;
    juce::OwnedArray<juce::ToggleButton> leds;
    bool showTitle, isBool;
    std::function<void()> before;
};

/** Algorithm selector: combo box + up/down arrows, like GLydian. */
class AlgoSelector : public juce::Component {
public:
    AlgoSelector(GMajorProcessor& p, int param, std::function<void()> beforeChange, std::function<void(int)> onAlgo);
    void resized() override;
    int current() const { return binding.get(); }
private:
    ParamBinding binding;
    juce::ComboBox box;
    juce::ArrowButton up { "up", 0.75f, juce::Colours::black }, down { "down", 0.25f, juce::Colours::black };
    std::function<void()> before;
    std::function<void(int)> notify;
};

/** Small horizontal level meter fed from the processor. */
class Meter : public juce::Component {
public:
    explicit Meter(juce::String t) : title(std::move(t)) {}
    void setLevelDb(float db) { if (std::abs(db - level) > 0.2f) { level = db; repaint(); } }
    void paint(juce::Graphics&) override;
    float minDb = -60.0f, maxDb = 0.0f;
    bool reduction = false;   // draw from the right (gain reduction)
private:
    juce::String title;
    float level = -100.0f;
};

} // namespace ui
