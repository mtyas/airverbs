#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace airverbs
{

class History;

struct Preset
{
    juce::String name, category, author, description;
    bool factory = false;
    juce::File file;                                  // user presets only
    std::vector<std::pair<juce::String, float>> values; // param id -> plain value (missing = default)

    juce::String key() const { return (factory ? "factory:" : "user:") + category + "/" + name; }
};

/**
    Factory + user preset library. User presets are plain XML files (*.airverb) in
      macOS:   ~/Library/Audio/Presets/Airverbs
      Windows: %APPDATA%/Airverbs/Presets
      Linux:   ~/.config/Airverbs/Presets
    so they can be shared, synced and organised in sub-folders (sub-folder = category).
*/
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, History&);

    static juce::File getUserDirectory();
    static constexpr const char* extension = ".airverb";

    void rescan();
    const std::vector<Preset>& getAll() const { return presets; }
    juce::StringArray getCategories() const;

    bool load (int index);
    void loadInit();
    bool loadNext (int delta);  // +1 / -1, wraps
    bool saveUser (const juce::String& name, const juce::String& category, const juce::String& description);
    bool deleteUser (int index);
    bool renameUser (int index, const juce::String& newName);
    bool importFile (const juce::File&);
    bool exportCurrent (const juce::File&);

    int getCurrentIndex() const          { return currentIndex; }
    juce::String getCurrentName() const  { return currentName; }
    bool isDirty() const;

    bool isFavourite (const Preset&) const;
    void setFavourite (const Preset&, bool);

    juce::ValueTree toValueTree() const;           // remembered in the session
    void fromValueTree (const juce::ValueTree&);

    std::function<void()> onChange;

private:
    Preset capture (const juce::String& name, const juce::String& category, const juce::String& description) const;
    void apply (const Preset&);
    static bool writeFile (const Preset&, const juce::File&);
    static bool readFile (const juce::File&, Preset&);
    std::vector<float> currentPlainValues() const;
    void loadFavourites();
    void saveFavourites() const;

    juce::AudioProcessorValueTreeState& apvts;
    History& history;
    std::vector<Preset> presets;
    juce::StringArray favourites;
    int currentIndex = -1;
    juce::String currentName = "Init";
    std::vector<float> loadedSnapshot;
};

std::vector<Preset> createFactoryPresets();

} // namespace airverbs
