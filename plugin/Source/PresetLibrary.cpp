#include "PresetLibrary.h"

void PresetLibrary::add(const gmr::Patch& p) {
    for (auto& e : list)
        if (e.name == p.name) { e = p; sendChangeMessage(); return; }
    list.push_back(p);
    sendChangeMessage();
}

void PresetLibrary::remove(int index) {
    if (index < 0 || index >= size()) return;
    list.erase(list.begin() + index);
    sendChangeMessage();
}

void PresetLibrary::clear() { list.clear(); sendChangeMessage(); }

void PresetLibrary::setAll(std::vector<gmr::Patch> patches) { list = std::move(patches); sendChangeMessage(); }

int PresetLibrary::readSyxFile(const juce::File& f, std::vector<gmr::Patch>& out, juce::String* error) {
    juce::MemoryBlock mb;
    if (!f.loadFileAsData(mb)) { if (error) *error = "Cannot read " + f.getFullPathName(); return 0; }
    const auto* d = static_cast<const uint8_t*>(mb.getData());
    const size_t len = mb.getSize();
    int count = 0;
    size_t pos = 0;
    gmr::Patch p;
    while (pos < len) {
        gmr::DecodeResult r;
        const size_t before = pos;
        if (gmr::readPresetAt(d, len, pos, p, &r)) { out.push_back(p); ++count; }
        else if (pos >= len || pos == before) break;
    }
    if (count == 0 && error) *error = "No G-Major preset found in " + f.getFileName();
    return count;
}

bool PresetLibrary::writeSyxFile(const juce::File& f, const std::vector<gmr::Patch>& patches) {
    juce::MemoryBlock mb;
    for (const auto& p : patches) {
        const auto bytes = gmr::encodeForFile(p);   // preset + optional extended-level message
        mb.append(bytes.data(), bytes.size());
    }
    return f.replaceWithData(mb.getData(), mb.getSize());
}

juce::String PresetLibrary::safeFileName(const juce::String& n) {
    auto s = juce::File::createLegalFileName(n.trim());
    return s.isEmpty() ? juce::String("Untitled") : s;
}

int PresetLibrary::loadFolder(const juce::File& dir) {
    std::vector<gmr::Patch> all;
    for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.syx")) readSyxFile(f, all);
    if (!all.empty()) setAll(std::move(all));
    return (int)list.size();
}

int PresetLibrary::saveFolder(const juce::File& dir) const {
    int n = 0;
    for (const auto& p : list)
        if (writeSyxFile(dir.getChildFile(safeFileName(p.name) + ".syx"), { p })) ++n;
    return n;
}

int PresetLibrary::importFile(const juce::File& f) {
    std::vector<gmr::Patch> got;
    const int n = readSyxFile(f, got);
    for (auto& p : got) add(p);
    return n;
}
