#include "PluginEditor.h"

using namespace gmr;

namespace {
enum MenuIds {
    idNew = 1, idOpen, idSave, idExportBank, idImport,
    idLibLoadFolder = 100, idLibSaveFolder, idLibAdd, idLibClear,
    idLibEntry = 1000, idLibRemove = 3000,
    idTxPlaying = 200, idRxPlaying, idTxBank, idRxBank, idTxRealtime, idProgChange, idRelayCc, idTxLibrary, idRxAllUser,
    idChannel = 300, idLearn = 400,
    idAbout = 500
};
}

GMajorEditor::GMajorEditor(GMajorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), menuBar(this), patchPanel(p),
      gate(p, Block::Gate), comp(p, Block::Comp), eq(p),
      filter(p, Block::Filter), pitch(p, Block::Pitch), mod(p, Block::Mod), delay(p, Block::Delay), reverb(p, Block::Reverb),
      live(p) {
    setLookAndFeel(&lnf);
    addAndMakeVisible(menuBar);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &patchPanel, &gate, &eq, &comp, &filter, &pitch, &mod, &delay, &reverb, &live })
        addAndMakeVisible(c);
    patchPanel.onOpenDisk = [this] { openSyx(); };
    patchPanel.onSaveDisk = [this] { saveSyx(); };
    patchPanel.onGetLib = [this] { loadLibraryFolder(); };
    proc.library.addChangeListener(this);
    proc.onPatchChanged = [safe = juce::Component::SafePointer<GMajorEditor>(this)] {
        if (safe) safe->refreshAll();
    };
    setSize(kWidth, kHeight);
    startTimerHz(5);
}

GMajorEditor::~GMajorEditor() {
    proc.onPatchChanged = nullptr;
    proc.library.removeChangeListener(this);
    menuBar.setModel(nullptr);
    setLookAndFeel(nullptr);
}

void GMajorEditor::refreshAll() {
    for (auto* s : { &gate, &comp, &filter, &pitch, &mod, &delay, &reverb }) s->refresh();
    eq.refresh();
    patchPanel.refresh();
}

void GMajorEditor::timerCallback() {
    // modifier assignments can change without a parameter change (undo, library copy, MIDI)
    for (auto* s : { &filter, &pitch, &mod, &delay, &reverb }) s->refresh();
}

void GMajorEditor::paint(juce::Graphics& g) { g.fillAll(ui::colours::window); }

void GMajorEditor::resized() {
    auto r = getLocalBounds();
    menuBar.setBounds(r.removeFromTop(22));
    live.setBounds(r.removeFromBottom(86));
    r.reduce(2, 2);
    auto left = r.removeFromLeft(322);
    r.removeFromLeft(3);
    patchPanel.setBounds(left.removeFromTop(186));
    left.removeFromTop(3);
    gate.setBounds(left.removeFromTop(gate.preferredHeight())); left.removeFromTop(3);
    eq.setBounds(left.removeFromTop(eq.preferredHeight()));     left.removeFromTop(3);
    comp.setBounds(left.removeFromTop(comp.preferredHeight()));
    const int h = (r.getHeight() - 4 * 3) / 5;
    for (auto* s : { &filter, &pitch, &mod, &delay, &reverb }) { s->setBounds(r.removeFromTop(h)); r.removeFromTop(3); }
}

// ============================================================ menus
juce::PopupMenu GMajorEditor::getMenuForIndex(int index, const juce::String&) {
    juce::PopupMenu m;
    if (index == 0) {
        m.addItem(idNew, "New (init patch)");
        m.addItem(idOpen, "Open .syx...");
        m.addItem(idSave, "Save patch as .syx...");
        m.addSeparator();
        m.addItem(idImport, "Import .syx file into library...");
        m.addItem(idExportBank, "Export library as one bank .syx...");
    } else if (index == 1) {
        m.addItem(idLibLoadFolder, "Load library from folder...");
        m.addItem(idLibSaveFolder, "Save library to folder...");
        m.addItem(idLibAdd, "Add current patch");
        m.addItem(idLibClear, "Clear library");
        m.addSeparator();
        m.addItem(idRxAllUser, "Receive all user presets from G-Major (1-100)");
        m.addItem(idTxLibrary, "Send library user presets to G-Major");
        m.addSeparator();
        juce::PopupMenu remove;
        for (int i = 0; i < proc.library.size(); ++i) {
            const Patch* p = proc.library.get(i);
            juce::String label = juce::String(p->name);
            if (p->bank() == Bank::User) label << "   (U" << p->numberInBank() << ")";
            else if (p->bank() == Bank::Factory) label << "   (F" << p->numberInBank() << ")";
            m.addItem(idLibEntry + i, label, true, i == proc.getCurrentProgram());
            remove.addItem(idLibRemove + i, juce::String(p->name));
        }
        if (proc.library.size() > 0) { m.addSeparator(); m.addSubMenu("Remove", remove); }
    } else if (index == 2) {
        m.addItem(idTxPlaying, "Tx Playing (send patch to G-Major)");
        m.addItem(idRxPlaying, "Rx Playing (receive edit buffer)");
        m.addItem(idTxBank, "Tx Bank (store patch at selected bank/number)");
        m.addItem(idRxBank, "Rx Bank (receive patch at selected bank/number)");
        m.addItem(idTxRealtime, "Tx RealTime", true, proc.getTxRealtime());
        m.addSeparator();
        m.addItem(idProgChange, "Respond to Program Change", true, proc.programChangeEnabled.load());
        m.addItem(idRelayCc, "Send relays as CC 80/81", true, proc.relayCcOut.load());
        juce::PopupMenu ch;
        ch.addItem(idChannel, "Omni", true, proc.midiChannel.load() == 0);
        for (int c = 1; c <= 16; ++c) ch.addItem(idChannel + c, juce::String(c), true, proc.midiChannel.load() == c);
        m.addSubMenu("MIDI channel", ch);
        juce::PopupMenu learn;
        for (int i = 0; i < 4; ++i) learn.addItem(idLearn + i, "Learn CC for M" + juce::String(i + 1));
        m.addSubMenu("Modifiers", learn);
    } else {
        m.addItem(idAbout, "About G-Major Reborn...");
    }
    return m;
}

void GMajorEditor::menuItemSelected(int id, int) {
    if (id == idNew) { Patch p = initPatch(); for (int i = pInLevel; i < kNumParams; ++i) p.set(i, proc.getParamStored(i)); proc.loadPatch(p); }
    else if (id == idOpen) openSyx();
    else if (id == idSave) saveSyx();
    else if (id == idImport) importIntoLibrary();
    else if (id == idExportBank) exportBank();
    else if (id == idLibLoadFolder) loadLibraryFolder();
    else if (id == idLibSaveFolder) saveLibraryFolder();
    else if (id == idLibAdd) proc.library.add(proc.currentPatch());
    else if (id == idLibClear) proc.library.clear();
    else if (id >= idLibRemove && id < idLibRemove + 1000) proc.library.remove(id - idLibRemove);
    else if (id >= idLibEntry && id < idLibEntry + 1000) proc.setCurrentProgram(id - idLibEntry);
    else if (id == idTxPlaying) proc.txPlaying();
    else if (id == idRxPlaying) proc.rxPlaying();
    else if (id == idTxBank) proc.txBank();
    else if (id == idRxBank) proc.rxBank();
    else if (id == idTxLibrary) proc.txLibrary();
    else if (id == idRxAllUser) proc.rxAllUser();
    else if (id == idTxRealtime) { proc.setTxRealtime(!proc.getTxRealtime()); patchPanel.refresh(); }
    else if (id == idProgChange) proc.programChangeEnabled = !proc.programChangeEnabled.load();
    else if (id == idRelayCc) proc.relayCcOut = !proc.relayCcOut.load();
    else if (id >= idChannel && id <= idChannel + 16) proc.midiChannel = id - idChannel;
    else if (id >= idLearn && id < idLearn + 4) proc.learnTarget = id - idLearn;
    else if (id == idAbout) showAbout();
}

// ============================================================ files
juce::File GMajorEditor::lastDir() const {
    return workDir.isDirectory() ? workDir : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
}
void GMajorEditor::rememberDir(const juce::File& f) { workDir = f.isDirectory() ? f : f.getParentDirectory(); }

void GMajorEditor::openSyx() {
    chooser = std::make_unique<juce::FileChooser>("Open G-Major preset", lastDir(), "*.syx");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        const auto f = fc.getResult();
        if (!f.existsAsFile()) return;
        rememberDir(f);
        std::vector<Patch> got; juce::String err;
        if (PresetLibrary::readSyxFile(f, got, &err) == 0) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Open", err);
            return;
        }
        Patch first = got.front();
        for (int i = pInLevel; i < kNumParams; ++i) first.set(i, proc.getParamStored(i));
        proc.loadPatch(first);
        if (got.size() > 1) for (auto& p : got) proc.library.add(p);
    });
}

void GMajorEditor::saveSyx() {
    chooser = std::make_unique<juce::FileChooser>("Save G-Major preset",
        lastDir().getChildFile(PresetLibrary::safeFileName(proc.getPatchName()) + ".syx"), "*.syx");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("syx")) f = f.withFileExtension("syx");
        rememberDir(f);
        if (!PresetLibrary::writeSyxFile(f, { proc.currentPatch() }))
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save", "Could not write " + f.getFullPathName());
    });
}

void GMajorEditor::loadLibraryFolder() {
    chooser = std::make_unique<juce::FileChooser>("Choose a folder of .syx files", lastDir());
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
        const auto d = fc.getResult();
        if (!d.isDirectory()) return;
        rememberDir(d);
        if (proc.library.loadFolder(d) == 0)
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Library", "No G-Major .syx presets found in " + d.getFullPathName());
    });
}

void GMajorEditor::saveLibraryFolder() {
    chooser = std::make_unique<juce::FileChooser>("Save library: choose a folder", lastDir());
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
        const auto d = fc.getResult();
        if (!d.isDirectory()) return;
        rememberDir(d);
        const int n = proc.library.saveFolder(d);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Library", juce::String(n) + " presets written to " + d.getFullPathName());
    });
}

void GMajorEditor::exportBank() {
    chooser = std::make_unique<juce::FileChooser>("Export library bank", lastDir().getChildFile("GMajor_bank.syx"), "*.syx");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("syx")) f = f.withFileExtension("syx");
        rememberDir(f);
        PresetLibrary::writeSyxFile(f, proc.library.entries());
    });
}

void GMajorEditor::importIntoLibrary() {
    chooser = std::make_unique<juce::FileChooser>("Import .syx into library", lastDir(), "*.syx");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        const auto f = fc.getResult();
        if (!f.existsAsFile()) return;
        rememberDir(f);
        const int n = proc.library.importFile(f);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Library", juce::String(n) + " presets imported");
    });
}

void GMajorEditor::showAbout() {
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "G-Major Reborn " JucePlugin_VersionString,
        "Independent reconstruction of a TC Electronic G-Major style multi-effect, built from the owner's manual "
        "and from the preset format recovered from the GLydian editor.\n\n"
        "Presets are byte-compatible G-Major SysEx (.syx): they can be exchanged with the hardware over MIDI.\n\n"
        "The DSP algorithms are approximations written from the manual's descriptions. They have NOT been "
        "measured against a real unit; see KNOWN_DIFFERENCES.md.\n\n"
        "Not affiliated with or endorsed by TC Electronic.");
}
