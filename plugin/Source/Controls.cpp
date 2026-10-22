#include "Controls.h"

using namespace gmr;

namespace ui {

// ============================================================ LookAndFeel
LookAndFeel::LookAndFeel() {
    setColour(juce::PopupMenu::backgroundColourId, colours::panelHi);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::header);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, colours::dim);
    setColour(juce::TextButton::textColourOffId, colours::text);
    setColour(juce::TextButton::textColourOnId, colours::text);
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
    setColour(juce::TextEditor::textColourId, colours::text);
    setColour(juce::TextEditor::outlineColourId, colours::dim);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::ToggleButton::textColourId, colours::text);
    setColour(juce::ToggleButton::tickColourId, colours::led);
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxOutlineColourId, colours::dim);
    setColour(juce::ResizableWindow::backgroundColourId, colours::window);
    setColour(juce::AlertWindow::backgroundColourId, colours::panelHi);
    setColour(juce::AlertWindow::textColourId, colours::text);
    setColour(juce::ListBox::backgroundColourId, juce::Colours::white);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2.0f);
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);

    // cyan value arc (GLydian style) starting at the zero point for bipolar ranges
    const double lo = s.getMinimum(), hi = s.getMaximum();
    float zeroPos = 0.0f;
    if (lo < 0.0 && hi > 0.0) zeroPos = (float)((0.0 - lo) / (hi - lo));
    const float a0 = start + zeroPos * (end - start);
    juce::Path track;
    track.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xffb9bccb));
    g.strokePath(track, juce::PathStrokeType(2.5f));
    juce::Path arc;
    arc.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, juce::jmin(a0, angle), juce::jmax(a0, angle), true);
    g.setColour(colours::arc);
    g.strokePath(arc, juce::PathStrokeType(3.0f));

    // knob body
    const float kr = r - 5.0f;
    juce::ColourGradient body(juce::Colour(0xff6d6d78), c.x - kr * 0.5f, c.y - kr * 0.6f, juce::Colour(0xff121216), c.x + kr * 0.6f, c.y + kr, true);
    g.setGradientFill(body);
    g.fillEllipse(c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle(-1.2f, -kr + 2.0f, 2.4f, kr * 0.55f, 1.0f);
    g.setColour(juce::Colours::white);
    g.fillPath(ptr, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle style, juce::Slider& s) {
    if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s); return; }
    const auto track = juce::Rectangle<float>((float)x + (float)w * 0.5f - 3.0f, (float)y, 6.0f, (float)h);
    g.setColour(juce::Colour(0xffb9bccb)); g.fillRoundedRectangle(track, 3.0f);
    g.setColour(colours::arc); g.fillRoundedRectangle(track.withTop(pos), 3.0f);
    g.setColour(colours::led);
    g.fillRoundedRectangle(juce::Rectangle<float>((float)x + 2.0f, pos - 4.0f, (float)w - 4.0f, 8.0f), 2.0f);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) {
    const float box = 12.0f;
    const auto r = juce::Rectangle<float>(2.0f, ((float)b.getHeight() - box) * 0.5f, box, box);
    g.setColour(juce::Colours::white);
    g.fillRect(r);
    g.setColour(hl ? colours::header : colours::dim);
    g.drawRect(r, 1.0f);
    if (b.getToggleState()) {
        juce::Path tick;
        tick.startNewSubPath(r.getX() + 2.5f, r.getCentreY());
        tick.lineTo(r.getX() + 5.0f, r.getBottom() - 2.5f);
        tick.lineTo(r.getRight() - 2.0f, r.getY() + 2.0f);
        g.setColour(colours::led);
        g.strokePath(tick, juce::PathStrokeType(2.0f));
    }
    g.setColour(b.findColour(juce::ToggleButton::textColourId));
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawFittedText(b.getButtonText(), (int)r.getRight() + 4, 0, b.getWidth() - (int)r.getRight() - 4, b.getHeight(), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    juce::ColourGradient grad(on ? juce::Colour(0xff9fb3e6) : juce::Colour(0xfffbfbfd), 0, r.getY(),
                              on ? juce::Colour(0xff7a90cf) : juce::Colour(0xffd4d6df), 0, r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(down ? colours::header : (hl ? colours::dim.darker() : juce::Colour(0xff9a9cab)));
    g.drawRoundedRectangle(r, 2.5f, 1.0f);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) {
    g.setColour(juce::Colours::white);
    g.fillRect(0, 0, w, h);
    g.setColour(cb.findColour(juce::ComboBox::outlineColourId));
    g.drawRect(0, 0, w, h, 1);
}

// ============================================================ ParamBinding
ParamBinding::ParamBinding(GMajorProcessor& p, int idx, std::function<void(int)> onChange)
    : index(idx), proc(p), prm(p.apvts.getParameter(GMajorProcessor::paramId(idx))) {
    // No sendInitialUpdate(): owners are still being constructed here; they read the initial value themselves.
    att = std::make_unique<juce::ParameterAttachment>(*prm, [cb = std::move(onChange)](float v) { if (cb) cb((int)std::lround(v)); });
}
void ParamBinding::set(int stored) { att->setValueAsCompleteGesture((float)stored); }
int ParamBinding::get() const { return proc.getParamStored(index); }

// ============================================================ Knob
Knob::Knob(GMajorProcessor& p, int prmIndex, std::function<void()> beforeChange) : param(prmIndex), proc(p) {
    const ParamDef& d = paramDef(param);
    caption = d.label;
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity(juce::jlimit(150, 420, (d.hi - d.lo) / 4 + 150));
    slider.setVelocityBasedMode(false);
    slider.setDoubleClickReturnValue(true, d.def);
    slider.onDragStart = std::move(beforeChange);
    slider.onValueChange = [this] {
        char t[64]; formatValue(param, (int)std::lround(slider.getValue()), t, sizeof t);
        valueText = t; repaint();
    };
    slider.setPopupMenuEnabled(false);
    addAndMakeVisible(slider);
    att = std::make_unique<juce::SliderParameterAttachment>(*p.apvts.getParameter(GMajorProcessor::paramId(param)), slider);
    slider.onValueChange();
    modBtn.onClick = [this] { showModMenu(); };
    modBtn.setTooltip("Modifier: assign M1..M4 (expression pedal / MIDI CC) to this parameter");
    addChildComponent(modBtn);
}

void Knob::setModifierVisible(bool v) { modVisible = v; modBtn.setVisible(v); if (v) refreshModifier(); }

void Knob::refreshModifier() {
    if (!modVisible) return;
    const ParamDef& d = paramDef(param);
    const int off = modifierOffset(param, proc.getParamStored(juce::jmax(0, algoParamFor(d.block))));
    if (off < kFirstModOffset) { modBtn.setButtonText("-"); return; }
    const auto m = proc.getModifier((off - kFirstModOffset) / 4);
    modBtn.setButtonText(m.source ? "M" + juce::String(m.source) : juce::String("OFF"));
    modBtn.setToggleState(m.source != 0, juce::dontSendNotification);
}

void Knob::showModMenu() {
    const ParamDef& d = paramDef(param);
    const int off = modifierOffset(param, proc.getParamStored(juce::jmax(0, algoParamFor(d.block))));
    if (off < kFirstModOffset) return;
    const int slot = (off - kFirstModOffset) / 4;
    const auto m = proc.getModifier(slot);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(d.label) + " modifier");
    for (int s = 0; s <= 4; ++s) menu.addItem(1 + s, s == 0 ? "OFF" : "M" + juce::String(s), true, m.source == s);
    menu.addSeparator();
    juce::PopupMenu curve;
    curve.addItem(10, "Linear  (0 / 50 / 100 %)");
    curve.addItem(11, "Inverted  (100 / 50 / 0 %)");
    curve.addItem(12, "Slow start  (0 / 25 / 100 %)");
    curve.addItem(13, "Fast start  (0 / 75 / 100 %)");
    curve.addItem(14, "Custom Min / Mid / Max...");
    menu.addSubMenu("Response curve  (now " + juce::String(m.minOut) + " / " + juce::String(m.midOut) + " / " + juce::String(m.maxOut) + ")", curve);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&modBtn),
                       [safeThis = juce::Component::SafePointer<Knob>(this), slot, m](int r) {
        auto* self = safeThis.getComponent();
        if (self == nullptr) return;
        Modifier n = m;
        if (r >= 1 && r <= 5) n.source = (uint8_t)(r - 1);
        else if (r == 10) { n.minOut = 0; n.midOut = 50; n.maxOut = 100; }
        else if (r == 11) { n.minOut = 100; n.midOut = 50; n.maxOut = 0; }
        else if (r == 12) { n.minOut = 0; n.midOut = 25; n.maxOut = 100; }
        else if (r == 13) { n.minOut = 0; n.midOut = 75; n.maxOut = 100; }
        else if (r == 14) {
            auto* w = new juce::AlertWindow("Modifier response", "MinOut / MidOut / MaxOut (0-100 %)", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor("min", juce::String(m.minOut), "MinOut");
            w->addTextEditor("mid", juce::String(m.midOut), "MidOut");
            w->addTextEditor("max", juce::String(m.maxOut), "MaxOut");
            w->addButton("OK", 1); w->addButton("Cancel", 0);
            juce::Component::SafePointer<Knob> safe(self);
            w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, slot, m](int res) {
                if (res == 1 && safe) {
                    Modifier c = m;
                    c.minOut = (uint8_t)juce::jlimit(0, 100, w->getTextEditorContents("min").getIntValue());
                    c.midOut = (uint8_t)juce::jlimit(0, 100, w->getTextEditorContents("mid").getIntValue());
                    c.maxOut = (uint8_t)juce::jlimit(0, 100, w->getTextEditorContents("max").getIntValue());
                    safe->proc.setModifier(slot, c);
                    safe->refreshModifier();
                }
            }), true);
            return;
        } else return;
        self->proc.setModifier(slot, n);
        self->refreshModifier();
    });
}

void Knob::resized() {
    if (compact) {
        slider.setBounds(0, 0, 32, 32);
        modBtn.setBounds(0, 46, 32, 15);
        return;
    }
    slider.setBounds((getWidth() - 38) / 2, 0, 38, 38);
    modBtn.setBounds((getWidth() - 34) / 2, 66, 34, 15);
}

void Knob::paint(juce::Graphics& g) {
    if (compact) {
        g.setColour(colours::dim.darker());
        g.setFont(juce::Font(10.5f));
        g.drawFittedText(valueText, 34, 9, getWidth() - 34, 14, juce::Justification::centredLeft, 1, 0.6f);
        g.setColour(colours::text);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawFittedText(caption, 0, 32, getWidth(), 13, juce::Justification::centredLeft, 1, 0.7f);
        return;
    }
    int y = 38;
    if (caption.isNotEmpty()) {
        g.setColour(colours::text);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawFittedText(caption, 0, y, getWidth(), 13, juce::Justification::centred, 1, 0.4f);
        y += 13;
    }
    g.setFont(juce::Font(10.5f));
    g.setColour(colours::dim.darker());
    g.drawFittedText(valueText, 0, y, getWidth(), 13, juce::Justification::centred, 1, 0.45f);
}

// ============================================================ LedGroup
LedGroup::LedGroup(GMajorProcessor& p, int prmIndex, std::function<void()> beforeChange, bool title)
    : param(prmIndex),
      binding(p, prmIndex, [this](int v) {
          const ParamDef& d = paramDef(param);
          for (int i = 0; i < leds.size(); ++i)
              leds[i]->setToggleState(isBool ? v == 1 : (d.lo + i) == v, juce::dontSendNotification);
      }),
      showTitle(title), before(std::move(beforeChange)) {
    const ParamDef& d = paramDef(param);
    isBool = d.kind == Kind::Bool;
    const int count = isBool ? 1 : d.hi - d.lo + 1;
    for (int i = 0; i < count; ++i) {
        auto* b = leds.add(new juce::ToggleButton(isBool ? juce::String(d.label) : juce::String(d.choices[i]).toUpperCase()));
        b->setClickingTogglesState(false);
        b->onClick = [this, i] {
            if (before) before();
            const ParamDef& dd = paramDef(param);
            if (isBool) binding.set(binding.get() ? 0 : 1);
            else binding.set(dd.lo + i);
        };
        addAndMakeVisible(b);
    }
    if (isBool) showTitle = false;
    const int v = binding.get();
    for (int i = 0; i < leds.size(); ++i) leds[i]->setToggleState(isBool ? v == 1 : (d.lo + i) == v, juce::dontSendNotification);
}

void LedGroup::resized() {
    int y = showTitle ? 14 : 0;
    const int h = juce::jmin(18, getHeight() - y);
    for (auto* b : leds) { b->setBounds(0, y, getWidth(), h); y += h; }
}

int LedGroup::preferredWidth() const {
    const juce::Font f(11.0f);
    int w = showTitle ? f.boldened().getStringWidth(paramDef(param).label) : 0;
    for (auto* b : leds) w = juce::jmax(w, f.getStringWidth(b->getButtonText()) + 22);
    return w + 4;
}

void LedGroup::setTextColour(juce::Colour c, bool) {
    for (auto* b : leds) b->setColour(juce::ToggleButton::textColourId, c);
}

void LedGroup::paint(juce::Graphics& g) {
    if (!showTitle) return;
    g.setColour(colours::text);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(paramDef(param).label, 0, 0, getWidth(), 13, juce::Justification::centredLeft);
}

// ============================================================ AlgoSelector
AlgoSelector::AlgoSelector(GMajorProcessor& p, int prmIndex, std::function<void()> beforeChange, std::function<void(int)> onAlgo)
    : binding(p, prmIndex, [this](int v) { box.setSelectedId(v + 1, juce::dontSendNotification); if (notify) notify(v); }),
      before(std::move(beforeChange)), notify(std::move(onAlgo)) {
    const ParamDef& d = paramDef(prmIndex);
    for (int i = d.lo; i <= d.hi; ++i) box.addItem(d.choices[i - d.lo], i + 1);
    box.setSelectedId(binding.get() + 1, juce::dontSendNotification);
    box.onChange = [this] { if (before) before(); binding.set(box.getSelectedId() - 1); };
    up.onClick = [this, d] { if (before) before(); binding.set(binding.get() <= d.lo ? d.hi : binding.get() - 1); };
    down.onClick = [this, d] { if (before) before(); binding.set(binding.get() >= d.hi ? d.lo : binding.get() + 1); };
    addAndMakeVisible(box); addAndMakeVisible(up); addAndMakeVisible(down);
}

void AlgoSelector::resized() {
    auto r = getLocalBounds();
    auto arrows = r.removeFromRight(18);
    box.setBounds(r.reduced(0, 1));
    up.setBounds(arrows.removeFromTop(arrows.getHeight() / 2).reduced(2, 1));
    down.setBounds(arrows.reduced(2, 1));
}

// ============================================================ Meter
void Meter::paint(juce::Graphics& g) {
    auto r = getLocalBounds();
    g.setColour(colours::text);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(title, r.removeFromLeft(34), juce::Justification::centredLeft);
    auto bar = r.reduced(0, 3).toFloat();
    g.setColour(juce::Colour(0xff1c1c22));
    g.fillRect(bar);
    const float t = juce::jlimit(0.0f, 1.0f, (level - minDb) / (maxDb - minDb));
    juce::Rectangle<float> fill = reduction ? bar.withLeft(bar.getRight() - bar.getWidth() * t) : bar.withWidth(bar.getWidth() * t);
    g.setColour(reduction ? juce::Colour(0xffe6a234) : (level > -1.0f ? juce::Colour(0xffe8453c) : colours::arc));
    g.fillRect(fill.reduced(1.0f));
}

} // namespace ui
