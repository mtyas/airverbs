#include "PresetManager.h"
#include "../state/History.h"

namespace airverbs
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, History& h) : apvts (s), history (h)
{
    loadFavourites();
    rescan();
    loadedSnapshot = currentPlainValues();
}

juce::File PresetManager::getUserDirectory()
{
#if JUCE_MAC
    auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets/Airverbs");
#else
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Airverbs/Presets");
#endif
    dir.createDirectory();
    return dir;
}

static juce::File settingsFile()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Airverbs/settings.xml");
#else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Airverbs/settings.xml");
#endif
}

void PresetManager::rescan()
{
    const auto currentKey = currentIndex >= 0 && currentIndex < (int) presets.size() ? presets[(size_t) currentIndex].key() : juce::String();
    presets = createFactoryPresets();
    for (auto& p : presets) p.factory = true;

    auto dir = getUserDirectory();
    juce::Array<juce::File> files = dir.findChildFiles (juce::File::findFiles, true, juce::String ("*") + extension);
    files.sort();
    for (auto& f : files)
    {
        Preset p;
        if (readFile (f, p))
        {
            p.factory = false;
            p.file = f;
            if (p.category.isEmpty())
                p.category = f.getParentDirectory() == dir ? juce::String ("User") : f.getParentDirectory().getFileName();
            presets.push_back (std::move (p));
        }
    }
    currentIndex = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].key() == currentKey) currentIndex = (int) i;
    if (onChange) onChange();
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (auto& p : presets) cats.addIfNotAlreadyThere (p.category);
    return cats;
}

std::vector<float> PresetManager::currentPlainValues() const
{
    std::vector<float> v;
    for (auto* p : apvts.processor.getParameters())
        v.push_back (p->getValue());
    return v;
}

bool PresetManager::isDirty() const
{
    const auto now = currentPlainValues();
    if (now.size() != loadedSnapshot.size()) return true;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::abs (now[i] - loadedSnapshot[i]) > 1.0e-5f) return true;
    return false;
}

void PresetManager::apply (const Preset& preset)
{
    std::map<juce::String, float> values (preset.values.begin(), preset.values.end());
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        float norm = rp->getDefaultValue();
        if (auto it = values.find (rp->paramID); it != values.end())
            norm = rp->convertTo0to1 (it->second);
        if (std::abs (rp->getValue() - norm) > 1.0e-6f)
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }
    }
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    const auto& p = presets[(size_t) index];
    history.beginAction ("Load preset " + p.name);
    apply (p);
    history.commitNow();
    currentIndex = index;
    currentName = p.name;
    loadedSnapshot = currentPlainValues();
    if (onChange) onChange();
    return true;
}

void PresetManager::loadInit()
{
    history.beginAction ("Init");
    apply (Preset {});
    history.commitNow();
    currentIndex = -1;
    currentName = "Init";
    loadedSnapshot = currentPlainValues();
    if (onChange) onChange();
}

bool PresetManager::loadNext (int delta)
{
    if (presets.empty()) return false;
    const int n = (int) presets.size();
    const int start = currentIndex < 0 ? (delta > 0 ? -1 : 0) : currentIndex;
    return load (((start + delta) % n + n) % n);
}

Preset PresetManager::capture (const juce::String& name, const juce::String& category, const juce::String& description) const
{
    Preset p;
    p.name = name; p.category = category; p.description = description;
    p.author = juce::SystemStats::getFullUserName();
    for (auto* prm : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            p.values.push_back ({ rp->paramID, rp->convertFrom0to1 (rp->getValue()) });
    return p;
}

bool PresetManager::writeFile (const Preset& p, const juce::File& f)
{
    juce::XmlElement xml ("AirverbsPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", p.name);
    xml.setAttribute ("category", p.category);
    xml.setAttribute ("author", p.author);
    xml.setAttribute ("description", p.description);
    for (auto& [id, value] : p.values)
    {
        auto* e = xml.createNewChildElement ("PARAM");
        e->setAttribute ("id", id);
        e->setAttribute ("value", value);
    }
    f.getParentDirectory().createDirectory();
    return xml.writeTo (f);
}

bool PresetManager::readFile (const juce::File& f, Preset& p)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("AirverbsPreset")) return false;
    p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    p.category = xml->getStringAttribute ("category");
    p.author = xml->getStringAttribute ("author");
    p.description = xml->getStringAttribute ("description");
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        p.values.push_back ({ e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value") });
    return true;
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& category, const juce::String& description)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    const auto cat = category.trim().isEmpty() ? juce::String ("User") : category.trim();
    auto file = getUserDirectory().getChildFile (juce::File::createLegalFileName (cat)).getChildFile (clean + extension);
    auto p = capture (name.trim(), cat, description);
    if (! writeFile (p, file)) return false;
    rescan();
    for (size_t i = 0; i < presets.size(); ++i)
        if (! presets[i].factory && presets[i].file == file) currentIndex = (int) i;
    currentName = p.name;
    loadedSnapshot = currentPlainValues();
    if (onChange) onChange();
    return true;
}

bool PresetManager::deleteUser (int index)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory) return false;
    if (! presets[(size_t) index].file.moveToTrash()) return false;
    if (index == currentIndex) currentIndex = -1;
    rescan();
    return true;
}

bool PresetManager::renameUser (int index, const juce::String& newName)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory) return false;
    auto p = presets[(size_t) index];
    const auto clean = juce::File::createLegalFileName (newName.trim());
    if (clean.isEmpty()) return false;
    auto newFile = p.file.getSiblingFile (clean + extension);
    p.name = newName.trim();
    if (! writeFile (p, newFile)) return false;
    if (newFile != p.file) p.file.deleteFile();
    if (index == currentIndex) currentName = p.name;
    rescan();
    return true;
}

bool PresetManager::importFile (const juce::File& f)
{
    Preset p;
    if (! readFile (f, p)) return false;
    const auto cat = p.category.isEmpty() ? juce::String ("Imported") : p.category;
    if (! f.copyFileTo (getUserDirectory().getChildFile (juce::File::createLegalFileName (cat)).getChildFile (f.getFileName())))
        return false;
    rescan();
    return true;
}

bool PresetManager::exportCurrent (const juce::File& f)
{
    return writeFile (capture (currentName, "Exported", {}), f.withFileExtension (extension));
}

bool PresetManager::isFavourite (const Preset& p) const { return favourites.contains (p.key()); }

void PresetManager::setFavourite (const Preset& p, bool fav)
{
    if (fav) favourites.addIfNotAlreadyThere (p.key());
    else     favourites.removeString (p.key());
    saveFavourites();
    if (onChange) onChange();
}

void PresetManager::loadFavourites()
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
        if (auto* favs = xml->getChildByName ("FAVOURITES"))
            for (auto* e : favs->getChildIterator())
                favourites.add (e->getStringAttribute ("key"));
}

void PresetManager::saveFavourites() const
{
    juce::XmlElement xml ("AirverbsSettings");
    auto* favs = xml.createNewChildElement ("FAVOURITES");
    for (auto& k : favourites) favs->createNewChildElement ("FAV")->setAttribute ("key", k);
    settingsFile().getParentDirectory().createDirectory();
    xml.writeTo (settingsFile());
}

juce::ValueTree PresetManager::toValueTree() const
{
    juce::ValueTree v ("PRESET");
    v.setProperty ("name", currentName, nullptr);
    v.setProperty ("key", currentIndex >= 0 ? presets[(size_t) currentIndex].key() : juce::String(), nullptr);
    return v;
}

void PresetManager::fromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("PRESET")) return;
    currentName = v.getProperty ("name", "Init").toString();
    const auto key = v.getProperty ("key").toString();
    currentIndex = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].key() == key) currentIndex = (int) i;
    loadedSnapshot = currentPlainValues();
    if (onChange) onChange();
}

} // namespace airverbs
