#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RackEditor.h"

using namespace gmr;

namespace {
juce::String hostName(int p) {
    const ParamDef& d = paramDef(p);
    juce::String b = blockName(d.block);
    if (d.block == Block::Global || d.block == Block::Live) b = {};
    return (b.isEmpty() ? juce::String() : b + " ") + d.label;
}
constexpr int kTimerHz = 30;
constexpr int kOutPaceTicks = 11;   // ~370 ms between SysEx transfers (GLydian waits 350-450 ms)
}

namespace {
bool blockHasModifiers(Block b) { return algoParamFor(b) >= 0; }
bool blockHasTempo(Block b) { return b == Block::Filter || b == Block::Mod || b == Block::Delay; }
bool blockHasMix(Block b) { return algoParamFor(b) >= 0; }
const int kOnParams[] = { pGateOn, pEqOn, pCompOn, pFltOn, pPitOn, pModOn, pDlyOn, pRevOn };
}

bool GMajorProcessor::paramInRack(Block rk, int p) noexcept {
    if (rk == Block::Global) return true;
    const ParamDef& d = paramDef(p);
    if (d.block == rk) return true;
    switch (p) {
        case pInLevel: case pGlobalOut: case pBypass: return true;
        case pKillDry: return blockHasMix(rk);
        case pMod1: case pMod2: case pMod3: case pMod4: return blockHasModifiers(rk);
        case pTapTempo: case pTempoSource: return blockHasTempo(rk);
        default: return false;
    }
}

juce::String GMajorProcessor::rackTitle(Block b) {
    switch (b) {
        case Block::Gate: return "Noise Gate";   case Block::Eq: return "EQ";       case Block::Comp: return "Compressor";
        case Block::Filter: return "Filter/Mod"; case Block::Pitch: return "Pitch"; case Block::Mod: return "Chorus/Flange";
        case Block::Delay: return "Delay";       case Block::Reverb: return "Reverb";
        case Block::Global: case Block::Live: case Block::Count: break;
    }
    return "G-Major";
}

int GMajorProcessor::fixedValue(int p) const noexcept {
    if (p == pOutLevel) return 0;                    // racks: unity preset level (OUTPUT knob = pGlobalOut)
    if (p == pRouting) return 0;
    for (int on : kOnParams) if (p == on) return 0;  // every other block off
    return paramDef(p).def;
}

juce::AudioProcessorValueTreeState::ParameterLayout GMajorProcessor::createLayout(Block rk) {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i) {
        if (!paramInRack(rk, i)) continue;
        const ParamDef& d = paramDef(i);
        auto attrs = juce::AudioParameterIntAttributes()
            .withStringFromValueFunction([i](int v, int) { char t[64]; formatValue(i, v, t, sizeof t); return juce::String(t); })
            .withValueFromStringFunction([i](const juce::String& s) {
                const ParamDef& dd = paramDef(i);
                for (int v = dd.lo; v <= dd.hi; ++v) { char t[64]; formatValue(i, v, t, sizeof t); if (s.trim().equalsIgnoreCase(t)) return v; }
                return s.getIntValue();
            });
        int def = d.def;
        if (rk != Block::Global && i == pGlobalOut) def = 0;
        if (rk != Block::Global) for (int on : kOnParams) if (i == on) def = 1;   // a rack starts switched on
        const juce::String name = (rk != Block::Global && i == pGlobalOut) ? juce::String("OUTPUT") : hostName(i);
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { d.key, 1 }, name, d.lo, d.hi, def, attrs));
    }
    return layout;
}

GMajorProcessor::GMajorProcessor(Block rk)
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      rack(rk), apvts(*this, nullptr, "GMR", createLayout(rk)) {
    for (int i = 0; i < kNumParams; ++i) {
        raw[(size_t)i] = paramInRack(rk, i) ? apvts.getRawParameterValue(paramId(i)) : nullptr;   // racks expose a subset
        if (raw[(size_t)i] && i < pInLevel) apvts.addParameterListener(paramId(i), this);
    }
    const int defaultsCC[4] = { 11, 1, 4, 2 };   // M1 expression, M2 mod wheel, M3 foot, M4 breath
    for (int i = 0; i < 4; ++i) { modCC[(size_t)i] = defaultsCC[i]; pendingModValue[(size_t)i] = -1; }

    if (isRack()) {
        // rack programs: the bundled presets that use this block
        std::vector<Patch> mine;
        for (const auto& p : factoryPresets())
            for (int on : kOnParams)
                if (paramDef(on).block == rack && p.get(on) == 1) mine.push_back(p);
        library.setAll(std::move(mine));
        patchName = rackTitle(rack);
    } else {
        library.setAll(factoryPresets());
        if (auto* first = library.get(0)) loadPatch(*first, false);
    }
    startTimerHz(kTimerHz);
}

GMajorProcessor::~GMajorProcessor() {
    stopTimer();
    for (int i = 0; i < pInLevel; ++i) if (raw[(size_t)i]) apvts.removeParameterListener(paramId(i), this);
}

bool GMajorProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void GMajorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    for (int i = 0; i < kNumParams; ++i) eng.setParam(i, getParamStored(i));
    eng.prepare(sampleRate, samplesPerBlock);
    scratchMidi.ensureSize(8192);
    lastRelay[0] = lastRelay[1] = -1;
}

void GMajorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // ---------------- MIDI in: SysEx -> message thread, CC -> modifiers, PC -> presets
    const int chan = midiChannel.load();
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isSysEx()) { midiIn.push(m.getRawData(), m.getRawDataSize()); continue; }
        if (chan > 0 && m.getChannel() != chan) continue;
        if (m.isController()) {
            const int cc = m.getControllerNumber(), val = m.getControllerValue();
            const int learn = learnTarget.load();
            if (learn >= 0 && learn < 4) { modCC[(size_t)learn] = cc; learnTarget = -1; }
            for (int i = 0; i < 4; ++i)
                if (modCC[(size_t)i].load() == cc) pendingModValue[(size_t)i] = val;
        } else if (m.isProgramChange() && programChangeEnabled.load()) {
            pendingProgram = m.getProgramChangeNumber();
        }
    }
    midi.clear();

    // ---------------- parameters -> engine (lock-free)
    for (int i = 0; i < kNumParams; ++i) {
        const auto* r = raw[(size_t)i];
        int v = r ? (int)std::lround(r->load(std::memory_order_relaxed)) : fixedValue(i);
        if (i >= pMod1 && i <= pMod4) { const int pv = pendingModValue[(size_t)(i - pMod1)].load(); if (pv >= 0) v = pv; }
        eng.setParam(i, v);
    }
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            if (const auto bpm = pos->getBpm()) eng.setHostTempo(*bpm, true);
            else eng.setHostTempo(0, false);
        }
    }

    // ---------------- audio
    const int numIn = getTotalNumInputChannels();
    if (numIn == 0) buffer.clear();                          // no input bus: process silence (tails only)
    else for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, n);             // mono input -> both engine channels
    if (buffer.getNumChannels() >= 2) {
        eng.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n);
    } else if (buffer.getNumChannels() == 1) {
        // mono out: process a temporary stereo pair in chunks and fold down
        float tmpR[256];
        float* l = buffer.getWritePointer(0);
        for (int pos = 0; pos < n; pos += 256) {
            const int k = std::min(256, n - pos);
            std::memcpy(tmpR, l + pos, sizeof(float) * (size_t)k);
            eng.process(l + pos, tmpR, k);
            for (int j = 0; j < k; ++j) l[pos + j] = 0.5f * (l[pos + j] + tmpR[j]);
        }
    }

    // ---------------- MIDI out: queued SysEx + relay CCs
    midiOut.popAll([&midi](const uint8_t* d, int size) { midi.addEvent(d, size, 0); });
    if (relayCcOut.load() && !isRack()) {
        const int outCh = chan > 0 ? chan : 1;
        for (int r = 0; r < 2; ++r) {
            const int v = (int)std::lround(raw[(size_t)(pRelay1 + r)]->load());
            if (v != lastRelay[r]) { midi.addEvent(juce::MidiMessage::controllerEvent(outCh, 80 + r, v ? 127 : 0), 0); lastRelay[r] = v; }
        }
    }
}

// ============================================================ model
void GMajorProcessor::setParamStored(int p, int stored) {
    if (auto* prm = apvts.getParameter(paramId(p))) {
        const float norm = prm->convertTo0to1((float)stored);
        if (std::abs(prm->getValue() - norm) > 1e-7f) prm->setValueNotifyingHost(norm);
    }
}

int GMajorProcessor::getParamStored(int p) const {
    const auto* r = raw[(size_t)p];
    return r ? (int)std::lround(r->load()) : fixedValue(p);
}

void GMajorProcessor::setModifier(int slot, const Modifier& m) {
    eng.setModifier(slot, m);
    dirtyForRealtime = true;
}

gmr::Patch GMajorProcessor::currentPatch() const {
    Patch p;
    for (int i = 0; i < kNumParams; ++i) p.set(i, getParamStored(i));
    for (int s = 0; s < kNumModSlots; ++s) p.mods[(size_t)s] = eng.getModifier(s);
    p.name = patchName.toStdString();
    p.number = patchNumber;
    p.hasRaw = hasRawImage;
    p.raw = rawImage;
    return p;
}

void GMajorProcessor::loadPatch(const gmr::Patch& p, bool notifyHost) {
    juce::ignoreUnused(notifyHost);   // parameters always notify so automation lanes stay in sync
    for (int i = 0; i < pInLevel; ++i) setParamStored(i, p.get(i));
    for (int s = 0; s < kNumModSlots; ++s) eng.setModifier(s, p.mods[(size_t)s]);
    patchName = juce::String(p.name);
    patchNumber = p.number;
    hasRawImage = p.hasRaw;
    rawImage = p.raw;
    dirtyForRealtime = true;
    if (onPatchChanged) onPatchChanged();
}

void GMajorProcessor::setPatchName(const juce::String& n) { patchName = n.substring(0, 20); dirtyForRealtime = true; }
void GMajorProcessor::setPatchNumber(int abs) { patchNumber = juce::jlimit(0, 200, abs); }

void GMajorProcessor::parameterChanged(const juce::String&, float) { dirtyForRealtime = true; }

// ============================================================ programs
int GMajorProcessor::getNumPrograms() { return juce::jmax(1, library.size()); }

void GMajorProcessor::setCurrentProgram(int index) {
    if (auto* p = library.get(index)) { currentProgram = index; loadPatch(*p); }
}

const juce::String GMajorProcessor::getProgramName(int index) {
    if (auto* p = library.get(index)) return juce::String(p->name);
    return "Init";
}

// ============================================================ hardware communication
void GMajorProcessor::queueOut(const uint8_t* d, int size) {
    outJobs.push_back({ std::vector<uint8_t>(d, d + size) });
}

void GMajorProcessor::txPlaying() {
    uint8_t m[kSysexSize];
    const Patch cur = currentPatch();
    encodeSysex(cur, m, true);
    if (midiOut.push(m, kSysexSize))
        setStatus("Tx: sent \"" + patchName + "\" to edit buffer" + (encodeExtension(cur).empty() ? juce::String()
                  : juce::String("  (levels above 0 dB limited to 0 dB: G-Major range)")));
}

void GMajorProcessor::txBank() {
    const Patch p = currentPatch();
    uint8_t m[kSysexSize];
    encodeSysex(p, m, p.bank() != Bank::User);    // GLydian: FACTORY bank is sent as an external (edit-buffer) patch
    if (midiOut.push(m, kSysexSize))
        setStatus(p.bank() == Bank::User ? "Tx Bank: stored \"" + patchName + "\" as user " + juce::String(p.numberInBank())
                                         : juce::String("Tx Bank: factory bank is read-only, sent to edit buffer"));
}

void GMajorProcessor::rxPlaying() {
    const auto rq = makePresetRequest(0);
    expectLoadNumber = 0;
    midiOut.push(rq.data(), (int)rq.size());
    setStatus("Rx: requested edit buffer");
}

void GMajorProcessor::rxBank() {
    const int n = juce::jmax(1, patchNumber);
    const auto rq = makePresetRequest(n);
    expectLoadNumber = n;
    midiOut.push(rq.data(), (int)rq.size());
    Patch tmp; tmp.number = n;
    setStatus(juce::String("Rx Bank: requested ") + (tmp.bank() == Bank::User ? "user " : "factory ") + juce::String(tmp.numberInBank()));
}

void GMajorProcessor::txLibrary() {
    int count = 0;
    for (const auto& p : library.entries()) {
        if (p.bank() != Bank::User) continue;
        uint8_t m[kSysexSize]; encodeSysex(p, m);
        queueOut(m, kSysexSize); ++count;
    }
    rxBankNext = -1;
    setStatus("Library -> G-Major: " + juce::String(count) + " user presets queued");
}

void GMajorProcessor::rxAllUser() {
    for (int n = 1; n <= 100; ++n) {
        const auto rq = makePresetRequest(Patch::absoluteNumber(Bank::User, n));
        queueOut(rq.data(), (int)rq.size());
    }
    rxBankNext = 0;
    setStatus("G-Major -> Library: requesting 100 user presets...");
}

void GMajorProcessor::handleIncomingSysex(const uint8_t* d, int size) {
    if (size < kSysexSize) return;
    Patch p;
    for (int i = pInLevel; i < kNumParams; ++i) p.set(i, getParamStored(i));
    const auto r = decodeSysex(d, (size_t)size, p);
    if (!r.ok) return;
    if (p.number == 0 || p.number == expectLoadNumber.load()) {
        expectLoadNumber = -1;
        loadPatch(p);
        setStatus(juce::String("Rx: received edit buffer \"") + p.name + "\"" + (r.checksumOk ? "" : " (checksum mismatch)"));
    } else {
        library.add(p);
        ++rxBankNext;
        setStatus("Rx: preset " + juce::String(p.numberInBank()) + " \"" + juce::String(p.name) + "\" added to library");
    }
}

void GMajorProcessor::timerCallback() {
    midiIn.popAll([this](const uint8_t* d, int size) { handleIncomingSysex(d, size); });

    for (int i = 0; i < 4; ++i) {
        const int v = pendingModValue[(size_t)i].load();
        if (v >= 0) { setParamStored(pMod1 + i, v); int expected = v; pendingModValue[(size_t)i].compare_exchange_strong(expected, -1); }
    }
    const int pc = pendingProgram.exchange(-1);
    if (pc >= 0) setCurrentProgram(pc);

    if (txRealtime && dirtyForRealtime.load()) {
        if (++realtimeHoldoff >= 5) { dirtyForRealtime = false; realtimeHoldoff = 0; txPlaying(); }
    } else realtimeHoldoff = 0;

    if (!outJobs.empty() && ++outPace >= kOutPaceTicks) {
        outPace = 0;
        auto& j = outJobs.front();
        if (midiOut.push(j.bytes.data(), (int)j.bytes.size())) outJobs.pop_front();
        if (outJobs.empty()) setStatus(rxBankNext >= 0 ? "G-Major -> Library: requests sent, " + juce::String(rxBankNext) + " presets received"
                                                       : juce::String("Library -> G-Major: done"));
    }
}

// ============================================================ state
void GMajorProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    juce::ValueTree meta("GMR_META");
    meta.setProperty("name", patchName, nullptr);
    meta.setProperty("number", patchNumber, nullptr);
    if (hasRawImage) meta.setProperty("raw", juce::Base64::toBase64(rawImage.data(), rawImage.size()), nullptr);
    juce::StringArray mods;
    for (int s = 0; s < kNumModSlots; ++s) {
        const auto m = eng.getModifier(s);
        mods.add(juce::String(m.source) + "," + juce::String(m.minOut) + "," + juce::String(m.midOut) + "," + juce::String(m.maxOut));
    }
    meta.setProperty("mods", mods.joinIntoString(";"), nullptr);
    for (int i = 0; i < 4; ++i) meta.setProperty("cc" + juce::String(i + 1), modCC[(size_t)i].load(), nullptr);
    meta.setProperty("midiChannel", midiChannel.load(), nullptr);
    meta.setProperty("programChange", programChangeEnabled.load(), nullptr);
    meta.setProperty("relayCc", relayCcOut.load(), nullptr);
    meta.setProperty("txRealtime", txRealtime, nullptr);
    juce::MemoryBlock lib;
    for (const auto& p : library.entries()) { const auto b = encodeForFile(p); lib.append(b.data(), b.size()); }
    meta.setProperty("library", lib.toBase64Encoding(), nullptr);
    state.removeChild(state.getChildWithName("GMR_META"), nullptr);
    state.appendChild(meta, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void GMajorProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    const auto meta = state.getChildWithName("GMR_META");
    apvts.replaceState(state);
    if (!meta.isValid()) return;
    patchName = meta.getProperty("name", "Init").toString();
    patchNumber = (int)meta.getProperty("number", 0);
    hasRawImage = false;
    if (meta.hasProperty("raw")) {
        juce::MemoryOutputStream mo;
        if (juce::Base64::convertFromBase64(mo, meta.getProperty("raw").toString()) && mo.getDataSize() == (size_t)kSysexSize) {
            std::memcpy(rawImage.data(), mo.getData(), (size_t)kSysexSize);
            hasRawImage = true;
        }
    }
    const auto mods = juce::StringArray::fromTokens(meta.getProperty("mods").toString(), ";", "");
    for (int s = 0; s < juce::jmin(mods.size(), kNumModSlots); ++s) {
        const auto f = juce::StringArray::fromTokens(mods[s], ",", "");
        if (f.size() == 4)
            eng.setModifier(s, Modifier { (uint8_t)f[0].getIntValue(), (uint8_t)f[1].getIntValue(), (uint8_t)f[2].getIntValue(), (uint8_t)f[3].getIntValue() });
    }
    for (int i = 0; i < 4; ++i) modCC[(size_t)i] = (int)meta.getProperty("cc" + juce::String(i + 1), modCC[(size_t)i].load());
    midiChannel = (int)meta.getProperty("midiChannel", 0);
    programChangeEnabled = (bool)meta.getProperty("programChange", true);
    relayCcOut = (bool)meta.getProperty("relayCc", false);
    txRealtime = (bool)meta.getProperty("txRealtime", false);
    juce::MemoryBlock lib;
    if (lib.fromBase64Encoding(meta.getProperty("library").toString()) && lib.getSize() >= (size_t)kSysexSize) {
        std::vector<Patch> list;
        const auto* d = static_cast<const uint8_t*>(lib.getData());
        size_t pos = 0;
        Patch p;
        while (pos < lib.getSize()) {
            const size_t before = pos;
            if (readPresetAt(d, lib.getSize(), pos, p)) list.push_back(p);
            else if (pos >= lib.getSize() || pos == before) break;
        }
        library.setAll(std::move(list));
    }
    if (onPatchChanged) onPatchChanged();
}

juce::AudioProcessorEditor* GMajorProcessor::createEditor() {
    if (isRack()) return new RackEditor(*this);
    return new GMajorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
#ifdef GMR_RACK_BLOCK
    return new GMajorProcessor((gmr::Block)GMR_RACK_BLOCK);
#else
    return new GMajorProcessor();
#endif
}
