#include "RackEditor.h"

using namespace gmr;

RackEditor::RackEditor(GMajorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), input(p, pInLevel), output(p, pGlobalOut), bypass(p, pBypass, {}, false) {
    setLookAndFeel(&lnf);
    if (p.rack == Block::Eq) section = std::make_unique<ui::EqSection>(p);
    else section = std::make_unique<ui::EffectSection>(p, p.rack);
    addAndMakeVisible(*section);

    programs.setTextWhenNothingSelected("Presets...");
    programs.setTooltip("Bundled presets that use this block (only this block's settings are loaded)");
    programs.onChange = [this] { if (programs.getSelectedId() > 0) proc.setCurrentProgram(programs.getSelectedId() - 1); };
    addAndMakeVisible(programs);
    openBtn.setTooltip("Take this block's settings from a G-Major .syx preset file");
    openBtn.onClick = [this] { openSyx(); };
    addAndMakeVisible(openBtn);

    output.setCaption("OUTPUT");
    addAndMakeVisible(input); addAndMakeVisible(output); addAndMakeVisible(bypass);
    if (p.hasParam(pKillDry)) { killDry = std::make_unique<ui::LedGroup>(p, pKillDry, std::function<void()> {}, false); addAndMakeVisible(*killDry); }
    if (p.hasParam(pTapTempo)) {
        tapTempo = std::make_unique<ui::Knob>(p, pTapTempo);
        tempoSrc = std::make_unique<ui::LedGroup>(p, pTempoSource, std::function<void()> {}, true);
        tapBtn.setTooltip("Tap the tempo (quarter notes)");
        tapBtn.onClick = [this] { tap(); };
        addAndMakeVisible(*tapTempo); addAndMakeVisible(*tempoSrc); addAndMakeVisible(tapBtn);
    }
    if (p.hasParam(pMod1)) {
        for (int i = 0; i < 4; ++i) {
            auto* s = mods.add(new juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox));
            s->setTooltip("Modifier M" + juce::String(i + 1) + ". Right-click the label to learn a MIDI CC.");
            addAndMakeVisible(s);
            modAtt.add(new juce::SliderParameterAttachment(*p.apvts.getParameter(GMajorProcessor::paramId(pMod1 + i)), *s));
            auto* l = modLabels.add(new juce::Label());
            l->setFont(juce::Font(10.0f, juce::Font::bold));
            l->setJustificationType(juce::Justification::centred);
            l->getProperties().set("mod", i);
            l->addMouseListener(this, false);
            addAndMakeVisible(l);
        }
    }
    addAndMakeVisible(inMeter); addAndMakeVisible(outMeter);
    if (p.rack == Block::Gate || p.rack == Block::Comp) {
        dynMeter = std::make_unique<ui::Meter>(p.rack == Block::Gate ? "GATE" : "GR");
        dynMeter->reduction = true; dynMeter->minDb = 0.0f; dynMeter->maxDb = p.rack == Block::Gate ? 60.0f : 24.0f;
        addAndMakeVisible(*dynMeter);
    }
    proc.library.addChangeListener(this);
    proc.onPatchChanged = [safe = juce::Component::SafePointer<RackEditor>(this)] { if (safe) safe->section->refresh(); };
    rebuildPrograms();

    const bool wide = algoParamFor(p.rack) >= 0;
    setSize(wide ? 780 : 450, kTop + section->preferredHeight() + kStrip);
    startTimerHz(20);
}

RackEditor::~RackEditor() {
    proc.onPatchChanged = nullptr;
    proc.library.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void RackEditor::rebuildPrograms() {
    programs.clear(juce::dontSendNotification);
    for (int i = 0; i < proc.library.size(); ++i) programs.addItem(juce::String(proc.library.get(i)->name), i + 1);
}

void RackEditor::openSyx() {
    chooser = std::make_unique<juce::FileChooser>("Take " + GMajorProcessor::rackTitle(proc.rack) + " settings from a .syx preset",
                                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.syx");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        const auto f = fc.getResult();
        if (!f.existsAsFile()) return;
        std::vector<Patch> got; juce::String err;
        if (PresetLibrary::readSyxFile(f, got, &err) == 0) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Open", err);
            return;
        }
        // prefer the first preset where this block is switched on
        int onP = -1;
        for (int on : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) if (paramDef(on).block == proc.rack) onP = on;
        const Patch* pick = &got.front();
        for (const auto& p : got) if (onP >= 0 && p.get(onP) == 1) { pick = &p; break; }
        proc.loadPatch(*pick);          // only parameters exposed by this rack are applied
    });
}

void RackEditor::tap() {
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (!taps.isEmpty() && now - taps.getLast() > 3000.0) taps.clear();
    taps.add(now);
    while (taps.size() > 5) taps.remove(0);
    if (taps.size() >= 2)
        proc.setParamStored(pTapTempo, juce::jlimit(100, 3000, (int)std::lround((taps.getLast() - taps.getFirst()) / (taps.size() - 1))));
}

void RackEditor::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == this || !e.eventComponent->getProperties().contains("mod")) return;
    const int i = (int)e.eventComponent->getProperties()["mod"];
    juce::PopupMenu m;
    m.addSectionHeader("Modifier M" + juce::String(i + 1) + " source");
    m.addItem(1, "Learn MIDI CC (move a pedal / controller)");
    m.addItem(3, "No MIDI CC (automation / slider only)");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent), [this, i](int r) {
        if (r == 1) proc.learnTarget = i;
        else if (r == 3) proc.modCC[(size_t)i] = -1;
    });
}

void RackEditor::timerCallback() {
    auto& e = proc.engine();
    inMeter.setLevelDb(dsp::gainToDb(e.inputPeak()));
    outMeter.setLevelDb(dsp::gainToDb(e.outputPeak()));
    if (dynMeter) dynMeter->setLevelDb(proc.rack == Block::Gate ? -e.gateGainDb() : -e.compReductionDb());
    const int learn = proc.learnTarget.load();
    for (int i = 0; i < modLabels.size(); ++i) {
        const int cc = proc.modCC[(size_t)i].load();
        modLabels[i]->setText(learn == i ? juce::String("LEARN") : "M" + juce::String(i + 1) + (cc >= 0 ? " CC" + juce::String(cc) : juce::String()),
                              juce::dontSendNotification);
    }
}

void RackEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::colours::window);
    auto top = getLocalBounds().removeFromTop(kTop);
    g.setColour(ui::colours::header);
    g.fillRect(top);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("G-Major " + GMajorProcessor::rackTitle(proc.rack) + " Rack", top.reduced(10, 0), juce::Justification::centredLeft);
    auto strip = getLocalBounds().removeFromBottom(kStrip);
    g.setColour(ui::colours::blueBox.interpolatedWith(ui::colours::panel, 0.5f));
    g.fillRect(strip);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRect(strip, 1);
}

void RackEditor::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(kTop).reduced(6, 4);
    openBtn.setBounds(top.removeFromRight(84));
    top.removeFromRight(6);
    const int titleW = juce::Font(15.0f, juce::Font::bold).getStringWidth("G-Major " + GMajorProcessor::rackTitle(proc.rack) + " Rack") + 14;
    top.removeFromLeft(titleW);
    programs.setBounds(top.removeFromRight(juce::jmin(220, top.getWidth())));
    auto strip = r.removeFromBottom(kStrip).reduced(6, 4);
    section->setBounds(r);

    input.setBounds(strip.removeFromLeft(ui::Knob::kWidth + 8).withHeight(ui::Knob::kHeightNoMod));
    output.setBounds(strip.removeFromLeft(ui::Knob::kWidth + 14).withTrimmedLeft(6).withHeight(ui::Knob::kHeightNoMod));
    auto col = strip.removeFromLeft(92);
    bypass.setBounds(col.removeFromTop(20));
    if (killDry) killDry->setBounds(col.removeFromTop(20));
    if (tapTempo) {
        tapTempo->setBounds(strip.removeFromLeft(ui::Knob::kWidth + 6).withHeight(ui::Knob::kHeightNoMod));
        auto c2 = strip.removeFromLeft(64);
        tapBtn.setBounds(c2.removeFromTop(20).withWidth(46));
        c2.removeFromTop(4);
        tempoSrc->setBounds(c2.withHeight(tempoSrc->preferredHeight()));
    }
    for (int i = 0; i < mods.size(); ++i) {
        auto c = strip.removeFromLeft(44);
        modLabels[i]->setBounds(c.removeFromBottom(14));
        mods[i]->setBounds(c.withSizeKeepingCentre(20, c.getHeight()));
    }
    strip.removeFromLeft(10);
    auto meters = strip.removeFromLeft(juce::jmin(200, strip.getWidth()));
    const int rows = dynMeter ? 3 : 2;
    const int h = juce::jmin(18, meters.getHeight() / rows);
    meters = meters.withSizeKeepingCentre(meters.getWidth(), h * rows);
    inMeter.setBounds(meters.removeFromTop(h));
    outMeter.setBounds(meters.removeFromTop(h));
    if (dynMeter) dynMeter->setBounds(meters.removeFromTop(h));
}
