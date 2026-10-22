#include "Sections.h"

using namespace gmr;

namespace ui {

namespace {
// Indexed by gmr::Block: Global, Gate, Eq, Comp, Filter, Pitch, Mod, Delay, Reverb, Live
constexpr int kOnParam[]  = { -1, pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn, -1 };
constexpr int kMixParam[] = { -1, -1, -1, -1, pFltMix, pPitMix, pModMix, pDlyMix, pRevMix, -1 };
constexpr int kOutParam[] = { -1, -1, -1, -1, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut, -1 };
/** Modifier slot range [first, last] owned by a block (CONFIRMED layout 28..155). */
constexpr int kModFirst[] = { 0, 0, 0, 0, 0, 5, 13, 20, 30, 0 };
constexpr int kModLast[]  = { -1, -1, -1, -1, 4, 12, 19, 29, 31, -1 };
int onParamFor(Block b) { return kOnParam[(int)b]; }
int mixParamFor(Block b) { return kMixParam[(int)b]; }
int outParamFor(Block b) { return kOutParam[(int)b]; }
std::pair<int, int> modSlotRange(Block b) { return { kModFirst[(int)b], kModLast[(int)b] }; }
void drawHeader(juce::Graphics& g, juce::Rectangle<int> r) {
    juce::ColourGradient grad(colours::header, (float)r.getX(), 0, juce::Colour(0xfff4f4f8), (float)r.getRight(), 0, false);
    grad.addColour(0.45, colours::header.interpolatedWith(juce::Colours::white, 0.35f));
    g.setGradientFill(grad);
    g.fillRect(r);
}
} // namespace

// ============================================================ EffectSection
EffectSection::EffectSection(GMajorProcessor& p, Block b) : block(b), proc(p) {
    auto before = [this] { snapshot(); };
    onLed = std::make_unique<LedGroup>(p, onParamFor(b), before, false);
    onLed->setTextColour(juce::Colours::white, true);
    addAndMakeVisible(*onLed);
    if (algoParamFor(b) >= 0) {
        algo = std::make_unique<AlgoSelector>(p, algoParamFor(b), before, [this](int) {
            juce::Component::SafePointer<EffectSection> safe(this);
            juce::MessageManager::callAsync([safe] { if (safe) safe->refresh(); });
        });
        addAndMakeVisible(*algo);
    }
    for (int i = 0; i < pInLevel; ++i) {
        const ParamDef& d = paramDef(i);
        if (d.block != b || i == onParamFor(b) || i == algoParamFor(b)) continue;
        if (d.kind == Kind::Int || d.kind == Kind::Table) {
            auto* k = knobs.add(new Knob(p, i, before));
            addAndMakeVisible(k);
            if (i == mixParamFor(b)) mixKnob = k;
            if (i == outParamFor(b)) outKnob = k;
        } else {
            addAndMakeVisible(groups.add(new LedGroup(p, i, before)));
        }
    }
    if (mixKnob) mixKnob->setCompact(true);
    if (outKnob) outKnob->setCompact(true);
    undoBtn.setTooltip("Undo the last edit in this block");
    editBtn.setTooltip("Copy this block's settings from a patch in the library");
    undoBtn.onClick = [this] { undo(); };
    editBtn.onClick = [this] { showEditMenu(); };
    addAndMakeVisible(undoBtn); addAndMakeVisible(editBtn);
    refresh();
}

int EffectSection::preferredHeight() const {
    if (block == Block::Eq) return kHeader + 18 + 3 * 60 + 6;
    if (block == Block::Gate || block == Block::Comp) return kHeader + Knob::kHeightNoMod + 8;
    return kHeader + 98;
}

std::vector<int> EffectSection::blockParams() const {
    std::vector<int> v;
    for (int i = 0; i < pInLevel; ++i) if (paramDef(i).block == block) v.push_back(i);
    return v;
}

void EffectSection::snapshot() {
    Snap s;
    for (int i : blockParams()) s.values.push_back(proc.getParamStored(i));
    const auto range = modSlotRange(block);
    for (int m = range.first; m <= range.second; ++m) s.mods.push_back(proc.getModifier(m));
    if (!history.empty() && history.back().values == s.values) return;
    history.push_back(std::move(s));
    if (history.size() > 64) history.erase(history.begin());
}

void EffectSection::undo() {
    if (history.empty()) return;
    const Snap s = history.back();
    history.pop_back();
    const auto params = blockParams();
    for (size_t i = 0; i < params.size() && i < s.values.size(); ++i) proc.setParamStored(params[i], s.values[i]);
    const auto range = modSlotRange(block);
    for (int m = range.first, k = 0; m <= range.second && k < (int)s.mods.size(); ++m, ++k) proc.setModifier(m, s.mods[(size_t)k]);
    refresh();
}

void EffectSection::showEditMenu() {
    juce::PopupMenu menu;
    menu.addSectionHeader("Copy " + juce::String(blockName(block)).toLowerCase() + " settings from:");
    const int onP = onParamFor(block);
    int count = 0;
    for (int i = 0; i < proc.library.size(); ++i) {
        const Patch* p = proc.library.get(i);
        if (!p || p->get(onP) != 1) continue;          // GLydian: only patches where the block is active
        juce::String item = juce::String(p->name);
        if (const int ap = algoParamFor(block); ap >= 0) item << "   [" << algorithmName(block, p->get(ap)) << "]";
        menu.addItem(100 + i, item);
        ++count;
    }
    if (count == 0) menu.addItem(1, "(no library patch uses this block)", false);
    menu.addSeparator();
    menu.addItem(2, "Reset block to defaults");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&editBtn), [this](int r) {
        if (r == 2) {
            snapshot();
            for (int i : blockParams()) if (i != onParamFor(block)) proc.setParamStored(i, paramDef(i).def);
            refresh();
            return;
        }
        const Patch* p = proc.library.get(r - 100);
        if (!p) return;
        snapshot();
        for (int i : blockParams()) proc.setParamStored(i, p->get(i));
        const auto range = modSlotRange(block);
        for (int m = range.first; m <= range.second; ++m) proc.setModifier(m, p->mods[(size_t)m]);
        refresh();
    });
}

void EffectSection::refresh() {
    const int a = algo ? algo->current() : 0;
    for (auto* k : knobs) {
        const ParamDef& d = paramDef(k->param);
        k->setVisible(paramUsedBy(d, a));
        k->setModifierVisible(modifierOffset(k->param, a) >= 0);
        if (k->param == pFltDepth) k->setCaption(a == algo::Panner ? "WIDTH" : "DEPTH");
    }
    for (auto* g : groups) g->setVisible(paramUsedBy(paramDef(g->param), a));
    resized();
    repaint();
}

void EffectSection::resized() {
    auto r = getLocalBounds();
    auto head = r.removeFromTop(kHeader);
    onLed->setBounds(head.removeFromLeft(160).reduced(4, 1));
    editBtn.setBounds(head.removeFromRight(30).reduced(3, 2));
    undoBtn.setBounds(head.removeFromRight(30).reduced(3, 2));
    layoutControls(r.reduced(4, 4));
}

void EffectSection::layoutControls(juce::Rectangle<int> area) {
    if (algo) {
        auto left = area.removeFromLeft(146);
        algo->setBounds(left.removeFromTop(22));
        left.removeFromTop(4);
        if (mixKnob) mixKnob->setBounds(left.removeFromLeft(Knob::kCompactWidth).withHeight(Knob::kCompactHeight));
        if (outKnob) outKnob->setBounds(left.removeFromLeft(Knob::kCompactWidth).withHeight(Knob::kCompactHeight));
        area.removeFromLeft(6);
    }
    int x = area.getX();
    const int y = area.getY();
    for (auto* g : groups) {
        if (!g->isVisible() || g->param != pGateMode) continue;
        g->setBounds(x, y, 70, g->preferredHeight()); x += 74;
    }
    for (auto* k : knobs) {
        if (!k->isVisible() || k == mixKnob || k == outKnob) continue;
        k->setBounds(x, y, Knob::kWidth, k->modifierVisible() ? Knob::kHeight : Knob::kHeightNoMod);
        x += Knob::kWidth + 2;
    }
    for (auto* g : groups) {
        if (!g->isVisible() || g->param == pGateMode) continue;
        const int w = g->preferredWidth();
        g->setBounds(x + 4, y, w, g->preferredHeight());
        x += w + 6;
    }
}

void EffectSection::paint(juce::Graphics& g) {
    auto r = getLocalBounds();
    juce::ColourGradient body(colours::panelHi, 0, (float)kHeader, colours::panel, 0, (float)r.getBottom(), false);
    g.setGradientFill(body);
    g.fillRect(r);
    drawHeader(g, r.removeFromTop(kHeader));
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRect(getLocalBounds(), 1);
}

// ============================================================ EqSection
void EqSection::layoutControls(juce::Rectangle<int> area) {
    const int colX = area.getX() + 46;
    const int rows[3][3] = { { pEq1Freq, pEq2Freq, pEq3Freq }, { pEq1Width, pEq2Width, pEq3Width }, { pEq1Gain, pEq2Gain, pEq3Gain } };
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            for (auto* k : knobs)
                if (k->param == rows[row][col]) {
                    k->setCaption("");
                    k->setBounds(colX + col * (Knob::kWidth + 14), area.getY() + 14 + row * 60, Knob::kWidth + 6, 54);
                }
}

void EqSection::paintOverChildren(juce::Graphics& g) {
    g.setColour(colours::text);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    const int x0 = 4 + 46, y0 = kHeader + 2;
    for (int c = 0; c < 3; ++c) g.drawText("BAND " + juce::String(c + 1), x0 + c * (Knob::kWidth + 14), y0 + 2, Knob::kWidth + 6, 12, juce::Justification::centred);
    const char* names[3] = { "FREQ", "WIDTH", "GAIN" };
    for (int r = 0; r < 3; ++r) g.drawText(names[r], 6, y0 + 28 + r * 60, 40, 14, juce::Justification::centredLeft);
}

// ============================================================ PatchPanel
PatchPanel::PatchPanel(GMajorProcessor& p)
    : proc(p), routing(p, pRouting, {}, true), outLevel(p, pOutLevel), tapTempo(p, pTapTempo),
      relay1(p, pRelay1, {}, false), relay2(p, pRelay2, {}, false) {
    for (auto* b : { &txPlaying, &txBank, &saveDisk, &addLib, &rxPlaying, &rxBank, &openDisk, &getLib }) addAndMakeVisible(b);
    txPlaying.setTooltip("Send the current patch to the G-Major edit buffer (MIDI out)");
    txBank.setTooltip("Store the current patch in the G-Major at the selected USER number (FACTORY = edit buffer)");
    rxPlaying.setTooltip("Request the G-Major's current edit buffer (MIDI in must be connected)");
    rxBank.setTooltip("Request the preset at the selected bank / number from the G-Major");
    saveDisk.setTooltip("Save the current patch as a G-Major .syx file");
    openDisk.setTooltip("Open a .syx file (first preset is loaded, all are added to the library)");
    addLib.setTooltip("Add the current patch to the library (same name = replace)");
    getLib.setTooltip("Load a library from a folder of .syx files (GLydian's button had no action)");
    txPlaying.onClick = [this] { proc.txPlaying(); };
    txBank.onClick = [this] { proc.txBank(); };
    rxPlaying.onClick = [this] { proc.rxPlaying(); };
    rxBank.onClick = [this] { proc.rxBank(); };
    addLib.onClick = [this] { proc.library.add(proc.currentPatch()); };
    saveDisk.onClick = [this] { if (onSaveDisk) onSaveDisk(); };
    openDisk.onClick = [this] { if (onOpenDisk) onOpenDisk(); };
    getLib.onClick = [this] { if (onGetLib) onGetLib(); };

    factory.setRadioGroupId(77); user.setRadioGroupId(77);
    auto setBank = [this](Bank b) { proc.setPatchNumber(Patch::absoluteNumber(b, (int)number.getValue())); refresh(); };
    factory.onClick = [setBank] { setBank(Bank::Factory); };
    user.onClick = [setBank] { setBank(Bank::User); };
    addAndMakeVisible(factory); addAndMakeVisible(user);
    name.setInputRestrictions(20);
    name.onTextChange = [this] { proc.setPatchName(name.getText()); };
    addAndMakeVisible(name);
    number.setSliderStyle(juce::Slider::IncDecButtons);
    number.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 34, 20);
    number.setRange(1, 100, 1);
    number.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    number.setColour(juce::Slider::textBoxTextColourId, colours::text);
    number.setColour(juce::Slider::textBoxHighlightColourId, colours::arc.withAlpha(0.4f));
    number.onValueChange = [this] { proc.setPatchNumber(Patch::absoluteNumber(user.getToggleState() ? Bank::User : Bank::Factory, (int)number.getValue())); };
    addAndMakeVisible(number);

    addAndMakeVisible(routing); addAndMakeVisible(outLevel); addAndMakeVisible(tapTempo);
    addAndMakeVisible(relay1); addAndMakeVisible(relay2);
    tap.setTooltip("Tap the tempo (quarter notes); used by every TEMPO parameter");
    tap.onClick = [this] {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (!taps.isEmpty() && now - taps.getLast() > 3000.0) taps.clear();
        taps.add(now);
        while (taps.size() > 5) taps.remove(0);
        if (taps.size() >= 2) {
            const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
            proc.setParamStored(pTapTempo, juce::jlimit(100, 3000, (int)std::lround(avg)));
        }
    };
    addAndMakeVisible(tap);
    txRealtime.setTooltip("Automatically transmit every edit to the G-Major edit buffer");
    txRealtime.onClick = [this] { proc.setTxRealtime(txRealtime.getToggleState()); };
    addAndMakeVisible(txRealtime);
    refresh();
    startTimerHz(4);
}

void PatchPanel::refresh() {
    if (!name.hasKeyboardFocus(true) && name.getText() != proc.getPatchName()) { name.setText(proc.getPatchName(), juce::dontSendNotification); name.setCaretPosition(0); }
    Patch tmp; tmp.number = proc.getPatchNumber();
    const Bank b = tmp.bank();
    user.setToggleState(b == Bank::User, juce::dontSendNotification);
    factory.setToggleState(b != Bank::User, juce::dontSendNotification);
    number.setValue(juce::jmax(1, tmp.numberInBank()), juce::dontSendNotification);
    txRealtime.setToggleState(proc.getTxRealtime(), juce::dontSendNotification);
}

void PatchPanel::timerCallback() { refresh(); }

void PatchPanel::resized() {
    auto r = getLocalBounds().reduced(4);
    auto rowA = r.removeFromTop(22); r.removeFromTop(3);
    auto rowB = r.removeFromTop(22); r.removeFromTop(6);
    const int bw = rowA.getWidth() / 4;
    for (auto* b : { &txPlaying, &txBank, &saveDisk, &addLib }) b->setBounds(rowA.removeFromLeft(bw).reduced(1, 0));
    for (auto* b : { &rxPlaying, &rxBank, &openDisk, &getLib }) b->setBounds(rowB.removeFromLeft(bw).reduced(1, 0));
    auto rowC = r.removeFromTop(22);
    factory.setBounds(rowC.removeFromLeft(78));
    user.setBounds(rowC.removeFromLeft(56));
    number.setBounds(rowC.removeFromRight(70));
    name.setBounds(rowC.reduced(2, 1));
    r.removeFromTop(8);
    auto box = r;
    routing.setBounds(box.removeFromLeft(104).reduced(4, 4).withHeight(routing.preferredHeight()));
    outLevel.setBounds(box.removeFromLeft(Knob::kWidth + 8).withTrimmedLeft(4).withHeight(Knob::kHeightNoMod).translated(0, 4));
    tapTempo.setBounds(box.removeFromLeft(Knob::kWidth + 12).withTrimmedLeft(8).withHeight(Knob::kHeightNoMod).translated(0, 4));
    auto right = box.reduced(4, 4);
    right.removeFromTop(12);
    relay1.setBounds(right.removeFromTop(18));
    relay2.setBounds(right.removeFromTop(18));
    right.removeFromTop(4);
    tap.setBounds(right.removeFromTop(18).withWidth(46));
    txRealtime.setBounds(outLevel.getX(), tapTempo.getBottom() + 2, 120, 18);
}

void PatchPanel::paint(juce::Graphics& g) {
    g.fillAll(colours::blueBox);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.fillRoundedRectangle(routing.getBounds().expanded(3).toFloat(), 4.0f);
    g.fillRoundedRectangle(relay1.getBounds().getUnion(relay2.getBounds()).expanded(3).withTop(relay1.getY() - 15).toFloat(), 4.0f);
    g.setColour(colours::text);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("RELAYS", relay1.getX(), relay1.getY() - 14, 80, 12, juce::Justification::centredLeft);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRect(getLocalBounds(), 1);
}

// ============================================================ LiveStrip
LiveStrip::LiveStrip(GMajorProcessor& p)
    : proc(p), input(p, pInLevel), globalOut(p, pGlobalOut),
      killDry(p, pKillDry, {}, false), bypass(p, pBypass, {}, false), tempoSrc(p, pTempoSource, {}, true) {
    addAndMakeVisible(input); addAndMakeVisible(globalOut);
    addAndMakeVisible(killDry); addAndMakeVisible(bypass); addAndMakeVisible(tempoSrc);
    for (int i = 0; i < 4; ++i) {
        auto* s = mods.add(new juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox));
        s->setTooltip("Modifier M" + juce::String(i + 1) + " (0-127). Right-click the label to learn a MIDI CC.");
        addAndMakeVisible(s);
        modAtt.add(new juce::SliderParameterAttachment(*p.apvts.getParameter(GMajorProcessor::paramId(pMod1 + i)), *s));
        auto* l = modLabels.add(new juce::Label());
        l->setFont(juce::Font(10.0f, juce::Font::bold));
        l->setJustificationType(juce::Justification::centred);
        l->addMouseListener(this, false);
        l->getProperties().set("mod", i);
        addAndMakeVisible(l);
    }
    for (auto* m : { &inMeter, &outMeter, &gateMeter, &compMeter }) addAndMakeVisible(m);
    gateMeter.reduction = compMeter.reduction = true;
    gateMeter.minDb = compMeter.minDb = 0.0f; gateMeter.maxDb = 60.0f; compMeter.maxDb = 24.0f;
    status.setFont(juce::Font(11.0f));
    status.setColour(juce::Label::textColourId, colours::dim.darker());
    addAndMakeVisible(status);
    startTimerHz(20);
}

void LiveStrip::timerCallback() {
    auto& e = proc.engine();
    inMeter.setLevelDb(dsp::gainToDb(e.inputPeak()));
    outMeter.setLevelDb(dsp::gainToDb(e.outputPeak()));
    gateMeter.setLevelDb(proc.getParamStored(pGateOn) ? -e.gateGainDb() : 0.0f);
    compMeter.setLevelDb(proc.getParamStored(pCompOn) ? -e.compReductionDb() : 0.0f);
    const int learn = proc.learnTarget.load();
    for (int i = 0; i < 4; ++i) {
        const int cc = proc.modCC[(size_t)i].load();
        modLabels[i]->setText(learn == i ? juce::String("LEARN") : "M" + juce::String(i + 1) + (cc >= 0 ? " CC" + juce::String(cc) : juce::String()),
                              juce::dontSendNotification);
    }
    status.setText(proc.lastMidiStatus(), juce::dontSendNotification);
}

void LiveStrip::resized() {
    auto r = getLocalBounds().reduced(6, 4);
    input.setBounds(r.removeFromLeft(Knob::kWidth + 8).withHeight(Knob::kHeightNoMod));
    globalOut.setBounds(r.removeFromLeft(Knob::kWidth + 14).withTrimmedLeft(6).withHeight(Knob::kHeightNoMod));
    auto col = r.removeFromLeft(96);
    killDry.setBounds(col.removeFromTop(20)); bypass.setBounds(col.removeFromTop(20));
    tempoSrc.setBounds(r.removeFromLeft(80).withHeight(tempoSrc.preferredHeight()));
    r.removeFromLeft(10);
    for (int i = 0; i < 4; ++i) {
        auto c = r.removeFromLeft(56);
        modLabels[i]->setBounds(c.removeFromBottom(14));
        mods[i]->setBounds(c.withSizeKeepingCentre(22, c.getHeight()));
    }
    r.removeFromLeft(14);
    auto meters = r.removeFromLeft(220);
    for (auto* m : { &inMeter, &outMeter, &gateMeter, &compMeter }) m->setBounds(meters.removeFromTop(16));
    r.removeFromLeft(10);
    status.setBounds(r);
}

void LiveStrip::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == this || !e.eventComponent->getProperties().contains("mod")) return;
    const int i = (int)e.eventComponent->getProperties()["mod"];
    juce::PopupMenu m;
    m.addSectionHeader("Modifier M" + juce::String(i + 1) + " source");
    m.addItem(1, "Learn MIDI CC (move a pedal / controller)");
    m.addItem(2, "Set CC number...");
    m.addItem(3, "No MIDI CC (automation / slider only)");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent), [this, i](int r) {
        if (r == 1) proc.learnTarget = i;
        else if (r == 3) proc.modCC[(size_t)i] = -1;
        else if (r == 2) {
            auto* w = new juce::AlertWindow("M" + juce::String(i + 1), "MIDI CC number (0-127)", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor("cc", juce::String(juce::jmax(0, proc.modCC[(size_t)i].load())));
            w->addButton("OK", 1); w->addButton("Cancel", 0);
            juce::Component::SafePointer<LiveStrip> safe(this);
            w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, i](int res) {
                if (res == 1 && safe) safe->proc.modCC[(size_t)i] = juce::jlimit(0, 127, w->getTextEditorContents("cc").getIntValue());
            }), true);
        }
    });
}

void LiveStrip::paint(juce::Graphics& g) {
    g.fillAll(colours::blueBox.interpolatedWith(colours::panel, 0.5f));
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRect(getLocalBounds(), 1);
}

} // namespace ui
