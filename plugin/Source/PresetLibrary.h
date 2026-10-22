#pragma once
#include <JuceHeader.h>
#include "gmr/Patch.h"
#include <vector>

/** Patch library (GLydian "Patch Library"): message-thread only. Stored as G-Major .syx files. */
class PresetLibrary : public juce::ChangeBroadcaster {
public:
    const std::vector<gmr::Patch>& entries() const noexcept { return list; }
    int size() const noexcept { return (int)list.size(); }
    const gmr::Patch* get(int i) const noexcept { return i >= 0 && i < size() ? &list[(size_t)i] : nullptr; }

    void add(const gmr::Patch& p);                  // replaces an entry with the same name (GLydian behaviour)
    void remove(int index);
    void clear();
    void setAll(std::vector<gmr::Patch> patches);

    /** Parse every G-Major preset found in a .syx file. Returns the number of presets read. */
    static int readSyxFile(const juce::File& f, std::vector<gmr::Patch>& out, juce::String* error = nullptr);
    static bool writeSyxFile(const juce::File& f, const std::vector<gmr::Patch>& patches);
    static juce::String safeFileName(const juce::String& patchName);

    int loadFolder(const juce::File& dir);         // every *.syx in the folder (replaces library)
    int saveFolder(const juce::File& dir) const;    // one <name>.syx per patch
    int importFile(const juce::File& f);            // append all presets of a file

private:
    std::vector<gmr::Patch> list;
};
