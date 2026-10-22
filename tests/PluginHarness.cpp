// Integration tests for the JUCE layer (processor + editor), run headless.
// Built with -DGMR_BUILD_HARNESS=ON on Linux/macOS/Windows; on Linux run under xvfb-run.
#include "../plugin/Source/PluginProcessor.h"
#include "../plugin/Source/PluginEditor.h"
#include "../plugin/Source/RackEditor.h"
#include <cstdio>

using namespace gmr;

static int failures = 0, checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; std::printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void pump(int ms = 120) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

static void process(GMajorProcessor& p, juce::MidiBuffer& midi, int n = 512) {
    juce::AudioBuffer<float> buf(2, n);
    juce::Random rnd(1);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) buf.setSample(c, i, rnd.nextFloat() * 0.4f - 0.2f);
    p.processBlock(buf, midi);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) CHECK(std::isfinite(buf.getSample(c, i)));
}

static void testStateRoundTrip() {
    std::printf("[state round trip]\n");
    GMajorProcessor a; a.prepareToPlay(48000, 512);
    a.setParamStored(pDlyTime, 777); a.setParamStored(pRevAlgo, algo::Plate); a.setParamStored(pKillDry, 1);
    a.setPatchName("State Test"); a.setPatchNumber(142);
    a.setModifier(7, Modifier { 3, 10, 40, 90 });
    a.modCC[2] = 21;
    a.library.add(a.currentPatch());
    juce::MemoryBlock mb; a.getStateInformation(mb);
    GMajorProcessor b; b.setStateInformation(mb.getData(), (int)mb.getSize());
    CHECK(b.getParamStored(pDlyTime) == 777);
    CHECK(b.getParamStored(pRevAlgo) == algo::Plate);
    CHECK(b.getParamStored(pKillDry) == 1);
    CHECK(b.getPatchName() == "State Test");
    CHECK(b.getPatchNumber() == 142);
    CHECK(b.getModifier(7) == (Modifier { 3, 10, 40, 90 }));
    CHECK(b.modCC[2].load() == 21);
    CHECK(b.library.size() == a.library.size());
}

static void testSysexInOut() {
    std::printf("[sysex in / out]\n");
    GMajorProcessor p; p.prepareToPlay(48000, 512);
    // incoming edit-buffer dump from a "G-Major"
    Patch hw = factoryPresets()[3]; hw.name = "From Hardware"; hw.number = 0;
    uint8_t m[kSysexSize]; encodeSysex(hw, m);
    juce::MidiBuffer midi; midi.addEvent(m, kSysexSize, 0);
    process(p, midi);
    pump();
    CHECK(p.getPatchName() == "From Hardware");
    CHECK(p.getParamStored(pFltAlgo) == hw.get(pFltAlgo));
    // incoming user-preset dump goes to the library
    const int before = p.library.size();
    hw.name = "User 55"; hw.number = Patch::absoluteNumber(Bank::User, 55); encodeSysex(hw, m);
    juce::MidiBuffer midi2; midi2.addEvent(m, kSysexSize, 0);
    process(p, midi2); pump();
    CHECK(p.library.size() == before + 1);
    // Tx Playing produces exactly the encoded edit-buffer message
    p.txPlaying();
    juce::MidiBuffer out; process(p, out);
    bool found = false;
    for (const auto meta : out) {
        const auto msg = meta.getMessage();
        if (msg.isSysEx() && msg.getRawDataSize() == kSysexSize) {
            uint8_t want[kSysexSize]; encodeSysex(p.currentPatch(), want, true);
            found = std::memcmp(msg.getRawData(), want, kSysexSize) == 0;
        }
    }
    CHECK(found);
    // Rx Playing sends the 10-byte request
    p.rxPlaying();
    juce::MidiBuffer out2; process(p, out2);
    bool rq = false;
    for (const auto meta : out2) { const auto msg = meta.getMessage(); rq |= msg.getRawDataSize() == 10 && msg.getRawData()[6] == 0x45; }
    CHECK(rq);
    // Rx Bank: request the selected bank/number, and the matching dump becomes the current patch
    p.setPatchNumber(Patch::absoluteNumber(Bank::User, 42));
    p.rxBank();
    juce::MidiBuffer out3; process(p, out3);
    bool rq42 = false;
    for (const auto meta : out3) { const auto msg = meta.getMessage(); const auto* d = msg.getRawData();
        rq42 |= msg.getRawDataSize() == 10 && d[6] == 0x45 && d[7] + 128 * d[8] == 142; }
    CHECK(rq42);
    hw.name = "Stored 42"; hw.number = 142; encodeSysex(hw, m);
    juce::MidiBuffer in3; in3.addEvent(m, kSysexSize, 0);
    process(p, in3); pump();
    CHECK(p.getPatchName() == "Stored 42");
    // Tx Bank: user bank -> message carries the preset number
    p.txBank();
    juce::MidiBuffer out4; process(p, out4);
    bool tx42 = false;
    for (const auto meta : out4) { const auto msg = meta.getMessage(); const auto* d = msg.getRawData();
        tx42 |= msg.getRawDataSize() == kSysexSize && d[7] + 128 * d[8] == 142; }
    CHECK(tx42);
}

static void testCcProgramChangeAndModifiers() {
    std::printf("[CC -> modifier, program change]\n");
    GMajorProcessor p; p.prepareToPlay(48000, 512);
    p.setCurrentProgram(9);                       // "Whammy Pedal (M1)" (M1 assigned to Pitch)
    CHECK(p.getParamStored(pPitAlgo) == algo::Whammy);
    juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, 127), 0);
    process(p, midi);
    CHECK(p.engine().getParam(pMod1) == 127);
    CHECK(std::abs(p.engine().effective(pWhamPitch) - 100.0f) < 0.01f);
    pump();
    CHECK(p.getParamStored(pMod1) == 127);        // reflected back into the host parameter
    juce::MidiBuffer pc; pc.addEvent(juce::MidiMessage::programChange(1, 4), 0);
    process(p, pc); pump();
    CHECK(p.getCurrentProgram() == 4);
    CHECK(p.getPatchName() == juce::String(factoryPresets()[4].name));
    // MIDI learn
    p.learnTarget = 1;
    juce::MidiBuffer lm; lm.addEvent(juce::MidiMessage::controllerEvent(1, 7, 64), 0);
    process(p, lm);
    CHECK(p.modCC[1].load() == 7 && p.learnTarget.load() == -1);
}

static void testExtendedLevels() {
    std::printf("[extended OUT LEVEL ranges]\n");
    GMajorProcessor p; p.prepareToPlay(48000, 512);
    for (int lv : { pOutLevel, pFltOut, pPitOut, pModOut, pDlyOut, pRevOut, pInLevel, pGlobalOut }) {
        auto* prm = dynamic_cast<juce::AudioParameterInt*>(p.apvts.getParameter(GMajorProcessor::paramId(lv)));
        CHECK(prm != nullptr && prm->getRange().getStart() == -50 && prm->getRange().getEnd() == 50);
    }
    // +dB survives the plugin's own .syx/library round trip, is clamped for the hardware
    p.setParamStored(pDlyOut, 24); p.setParamStored(pOutLevel, 6);
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gmr_ext_test.syx");
    CHECK(PresetLibrary::writeSyxFile(tmp, { p.currentPatch() }));
    std::vector<Patch> back; CHECK(PresetLibrary::readSyxFile(tmp, back) == 1);
    CHECK(back[0].get(pDlyOut) == 24 && back[0].get(pOutLevel) == 6);
    tmp.deleteFile();
    p.txPlaying(); juce::MidiBuffer out; process(p, out);
    for (const auto meta : out) {
        const auto msg = meta.getMessage();
        if (msg.getRawDataSize() == kSysexSize) { Patch hw; CHECK(decodeSysex(msg.getRawData(), kSysexSize, hw).ok); CHECK(hw.get(pDlyOut) == 0 && hw.get(pOutLevel) == 0); }
    }
}

static void testLayouts() {
    std::printf("[bus layouts / block sizes / rates]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        GMajorProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add(juce::AudioChannelSet::mono()); l.outputBuses.add(juce::AudioChannelSet::stereo());
        CHECK(p.setBusesLayout(l));
        p.prepareToPlay(sr, 1024);
        for (int n : { 1, 31, 64, 480, 1024 }) { juce::MidiBuffer m; process(p, m, n); }
    }
}

static void checkLayout(juce::Component& root, const char* tag, int& problems) {
    // Only our own containers are checked (JUCE widgets may legitimately overhang internally).
    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
        const bool ours = dynamic_cast<GMajorEditor*>(&c) || dynamic_cast<RackEditor*>(&c) || dynamic_cast<ui::EffectSection*>(&c)
                       || dynamic_cast<ui::PatchPanel*>(&c) || dynamic_cast<ui::LiveStrip*>(&c);
        if (!ours) return;
        for (auto* ch : c.getChildren()) {
            if (!ch->isVisible()) continue;
            if (!c.getLocalBounds().contains(ch->getBounds())) {
                ++problems;
                if (problems < 40) std::printf("  layout: %s '%s' %s outside parent %s\n", tag, ch->getName().toRawUTF8(),
                                               ch->getBounds().toString().toRawUTF8(), c.getLocalBounds().toString().toRawUTF8());
            }
            for (auto* other : c.getChildren())
                if (other != ch && other->isVisible() && other->getBounds().intersects(ch->getBounds())
                    && ch < other && dynamic_cast<ui::EffectSection*>(&c)) {
                    ++problems;
                    std::printf("  overlap: %s %s / %s\n", tag, ch->getBounds().toString().toRawUTF8(), other->getBounds().toString().toRawUTF8());
                }
            walk(*ch);
        }
    };
    walk(root);
}

static void testEditor(const juce::File& shotDir) {
    std::printf("[editor: build, every algorithm, layout bounds, snapshots]\n");
    GMajorProcessor p; p.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    CHECK(ed != nullptr);
    ed->setVisible(true);
    pump(200);
    CHECK(ed->getWidth() == GMajorEditor::kWidth && ed->getHeight() == GMajorEditor::kHeight);
    int problems = 0;
    for (Block b : { Block::Filter, Block::Pitch, Block::Mod, Block::Delay, Block::Reverb }) {
        const int ap = algoParamFor(b);
        for (int a = 0; a < numAlgorithms(b); ++a) {
            p.setParamStored(ap, a);
            pump(60);
            checkLayout(*ed, algorithmName(b, a), problems);
        }
    }
    CHECK(problems == 0);
    if (shotDir != juce::File()) {
        shotDir.createDirectory();
        const auto presets = factoryPresets();
        for (int i : { 1, 9, 10, 14 }) {
            p.setCurrentProgram(i); pump(150);
            const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = shotDir.getChildFile("editor_" + juce::String(i + 1).paddedLeft('0', 2) + ".png");
            f.deleteFile();
            juce::FileOutputStream os(f);
            CHECK(os.openedOk() && png.writeImageToStream(img, os));
        }
    }
    ed.reset();
}

static void testRacks(const juce::File& shotDir) {
    std::printf("[racks: parameters, audio isolation, programs, state, editors]\n");
    const Block racks[] = { Block::Gate, Block::Eq, Block::Comp, Block::Filter, Block::Pitch, Block::Mod, Block::Delay, Block::Reverb };
    for (Block b : racks) {
        GMajorProcessor p(b);
        CHECK(p.isRack());
        // exposes every parameter of its block, the unit controls it needs, nothing from other blocks
        for (int i = 0; i < kNumParams; ++i) {
            const ParamDef& d = paramDef(i);
            if (d.block == b) CHECK(p.hasParam(i));
            else if (d.block != Block::Live && i != pTapTempo) CHECK(!p.hasParam(i));
        }
        CHECK(p.hasParam(pInLevel) && p.hasParam(pGlobalOut) && p.hasParam(pBypass));
        CHECK(p.hasParam(pMod1) == (algoParamFor(b) >= 0));
        CHECK(p.getParameters().size() < 40);
        p.prepareToPlay(48000, 512);
        // the engine sees only this block switched on, unity preset level, serial routing
        juce::MidiBuffer m; process(p, m);
        for (int on : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn }) {
            const bool mine = paramDef(on).block == b;
            CHECK(p.engine().getParam(on) == (mine ? 1 : 0));
        }
        CHECK(p.engine().getParam(pOutLevel) == 0);
        // OUTPUT = -50..+50 dB
        p.setParamStored(pGlobalOut, 50); juce::MidiBuffer m2; process(p, m2);
        CHECK(p.engine().getParam(pGlobalOut) == 50);
        p.setParamStored(pGlobalOut, 0);
        // programs: only presets that use this block; loading one never touches other blocks
        for (int i = 0; i < p.getNumPrograms() && p.library.size() > 0; ++i) {
            p.setCurrentProgram(i);
            juce::MidiBuffer m3; process(p, m3);
            for (int on : { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn })
                if (paramDef(on).block != b) CHECK(p.engine().getParam(on) == 0);
        }
        // state round trip
        const int ap = algoParamFor(b);
        if (ap >= 0) p.setParamStored(ap, paramDef(ap).hi);
        juce::MemoryBlock mb; p.getStateInformation(mb);
        GMajorProcessor q(b); q.setStateInformation(mb.getData(), (int)mb.getSize());
        if (ap >= 0) CHECK(q.getParamStored(ap) == paramDef(ap).hi);
        // editor
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        CHECK(dynamic_cast<RackEditor*>(ed.get()) != nullptr);
        ed->setVisible(true); pump(80);
        int problems = 0;
        const int na = ap >= 0 ? numAlgorithms(b) : 1;
        for (int a = 0; a < na; ++a) {
            if (ap >= 0) { p.setParamStored(ap, a); pump(50); }
            checkLayout(*ed, GMajorProcessor::rackTitle(b).toRawUTF8(), problems);
        }
        CHECK(problems == 0);
        if (shotDir != juce::File()) {
            if (ap >= 0) { p.setParamStored(ap, paramDef(ap).def); pump(80); }
            const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            auto f = shotDir.getChildFile("rack_" + GMajorProcessor::rackTitle(b).removeCharacters("/ ") + ".png");
            f.deleteFile();
            juce::FileOutputStream os(f); juce::PNGImageFormat png;
            CHECK(os.openedOk() && png.writeImageToStream(img, os));
        }
    }
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File shots = argc > 1 ? juce::File(juce::String(argv[1])) : juce::File();
    testStateRoundTrip();
    testSysexInOut();
    testCcProgramChangeAndModifiers();
    testLayouts();
    testExtendedLevels();
    testEditor(shots);
    testRacks(shots);
    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
