#pragma once
#include "Sections.h"

class GMajorEditor final : public juce::AudioProcessorEditor,
                           private juce::MenuBarModel,
                           private juce::ChangeListener,
                           private juce::Timer {
public:
    explicit GMajorEditor(GMajorProcessor&);
    ~GMajorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 1090, kHeight = 722;

private:
    // MenuBarModel
    juce::StringArray getMenuBarNames() override { return { "File", "Patch Library", "Midi", "Help" }; }
    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override;
    void menuItemSelected(int id, int index) override;

    void changeListenerCallback(juce::ChangeBroadcaster*) override { menuItemsChanged(); }
    void timerCallback() override;
    void refreshAll();

    void openSyx();
    void saveSyx();
    void loadLibraryFolder();
    void saveLibraryFolder();
    void exportBank();
    void importIntoLibrary();
    void showAbout();
    juce::File lastDir() const;
    void rememberDir(const juce::File& f);

    GMajorProcessor& proc;
    ui::LookAndFeel lnf;
    juce::MenuBarComponent menuBar;
    ui::PatchPanel patchPanel;
    ui::EffectSection gate, comp;
    ui::EqSection eq;
    ui::EffectSection filter, pitch, mod, delay, reverb;
    ui::LiveStrip live;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File workDir;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GMajorEditor)
};
