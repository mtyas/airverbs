#include "PluginProcessor.h"
#include "gui/PluginEditor.h"

namespace airverbs
{

/** Builds new engines off the audio thread and frees retired ones. */
class AirverbsProcessor::LoaderThread : public juce::Thread
{
public:
    explicit LoaderThread (AirverbsProcessor& p) : juce::Thread ("Airverbs engine loader"), owner (p) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            owner.syncEngineRequests();
            const bool busy = owner.network.serviceLoaders();
            wait (busy ? 1 : 8);
        }
    }
private:
    AirverbsProcessor& owner;
};

AirverbsProcessor::AirverbsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AIRVERBS", createParameterLayout()),
      history (*this),
      midiLearn (*this),
      presets (apvts, history)
{
    auto raw = [this] (const juce::String& id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    mix = raw (ids::mix); output = raw (ids::output); input = raw (ids::input);
    topology = raw (ids::topology); morph = raw (ids::morph); vecX = raw (ids::vecX); vecY = raw (ids::vecY);
    orbitRate = raw (ids::orbitRate); orbitDepth = raw (ids::orbitDepth); orbitShape = raw (ids::orbitShape);
    size = raw (ids::size); decay = raw (ids::decay); tone = raw (ids::tone);
    feedback = raw (ids::feedback); freeze = raw (ids::freeze); duck = raw (ids::duck);
    width = raw (ids::width); lowCut = raw (ids::lowCut); highCut = raw (ids::highCut);
    for (int s = 0; s < numSlots; ++s)
    {
        auto& r = slotRefs[(size_t) s];
        r.engine = raw (ids::engine (s)); r.level = raw (ids::level (s)); r.pan = raw (ids::pan (s));
        r.width = raw (ids::slotWidth (s)); r.predelay = raw (ids::predelay (s)); r.mute = raw (ids::mute (s));
        for (int k = 0; k < EngineRegistry::maxControls; ++k) r.controls[(size_t) k] = raw (ids::control (s, k));
        network.slot (s).requestEngine ((int) r.engine->load());
    }
    network.prepare (44100.0, 512);

    loader = std::make_unique<LoaderThread> (*this);
    loader->startThread (juce::Thread::Priority::normal);
}

AirverbsProcessor::~AirverbsProcessor()
{
    loader->stopThread (2000);
}

void AirverbsProcessor::syncEngineRequests()
{
    for (int s = 0; s < numSlots; ++s)
        network.slot (s).requestEngine ((int) slotRefs[(size_t) s].engine->load());
}

void AirverbsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    syncEngineRequests();
    network.prepare (sampleRate, samplesPerBlock);
}

bool AirverbsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    return in.size() <= out.size();
}

void AirverbsProcessor::readParams (NetworkParams& p)
{
    p.mix = mix->load(); p.outputDb = output->load(); p.inputDb = input->load();
    p.topology = (Topology) juce::jlimit (0, (int) Topology::count - 1, (int) topology->load());
    p.morph = morph->load(); p.vecX = vecX->load(); p.vecY = vecY->load();
    p.orbitRate = orbitRate->load(); p.orbitDepth = orbitDepth->load();
    p.orbitShape = (OrbitShape) juce::jlimit (0, (int) OrbitShape::count - 1, (int) orbitShape->load());
    p.macros.size = size->load(); p.macros.decay = decay->load(); p.macros.tone = tone->load();
    p.feedback = feedback->load(); p.freeze = freeze->load() > 0.5f; p.duck = duck->load();
    p.width = width->load(); p.lowCut = lowCut->load(); p.highCut = highCut->load();
    for (int s = 0; s < numSlots; ++s)
    {
        auto& r = slotRefs[(size_t) s];
        auto& sp = p.slots[(size_t) s];
        sp.engine = (int) r.engine->load(); sp.levelDb = r.level->load(); sp.pan = r.pan->load();
        sp.width = r.width->load(); sp.predelayMs = r.predelay->load(); sp.mute = r.mute->load() > 0.5f;
        for (int k = 0; k < EngineRegistry::maxControls; ++k) sp.controls[(size_t) k] = r.controls[(size_t) k]->load();
    }
}

void AirverbsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midiLearn.processMidi (midi);

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    for (int ch = std::max (numIn, numIn == 1 && numOut == 2 ? 2 : numIn); ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    readParams (params);
    network.process (buffer.getArrayOfWritePointers(), numOut, buffer.getNumSamples(), params);
}

juce::AudioProcessorEditor* AirverbsProcessor::createEditor() { return new AirverbsEditor (*this); }

// ---- programs = factory presets, so hosts without our browser can still step through them
int AirverbsProcessor::getNumPrograms()
{
    int n = 0;
    for (auto& p : presets.getAll()) n += p.factory ? 1 : 0;
    return std::max (1, n);
}
int AirverbsProcessor::getCurrentProgram() { return std::max (0, presets.getCurrentIndex()); }
void AirverbsProcessor::setCurrentProgram (int i)
{
    if (i >= 0 && i < (int) presets.getAll().size() && presets.getAll()[(size_t) i].factory && i != presets.getCurrentIndex())
        presets.load (i);
}
const juce::String AirverbsProcessor::getProgramName (int i)
{
    return i >= 0 && i < (int) presets.getAll().size() ? presets.getAll()[(size_t) i].name : juce::String ("Init");
}

void AirverbsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree root ("AIRVERBS_STATE");
    root.setProperty ("version", JucePlugin_VersionString, nullptr);
    root.appendChild (state, nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    root.appendChild (presets.toValueTree(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void AirverbsProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("AIRVERBS_STATE")) return;

    auto state = root.getChildWithName (apvts.state.getType());
    if (state.isValid()) apvts.replaceState (state);
    midiLearn.fromValueTree (root.getChildWithName ("MIDI_MAP"));
    auto ui = root.getChildWithName ("UI");
    if (ui.isValid()) uiState.copyPropertiesAndChildrenFrom (ui, nullptr);

    // Build the session's engines right away so the first rendered block already uses them.
    syncEngineRequests();
    network.serviceLoaders();

    presets.fromValueTree (root.getChildWithName ("PRESET"));
    history.reset();
}

void AirverbsProcessor::setSlotEngine (int slot, int engineIndex)
{
    const auto& reg = EngineRegistry::get();
    engineIndex = reg.clampIndex (engineIndex);
    const auto& info = reg.info (engineIndex);
    history.beginAction ("Star " + ids::slotName (slot) + " engine " + juce::String (info.name));

    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        }
    };
    set (ids::engine (slot), (float) engineIndex);
    for (int k = 0; k < EngineRegistry::maxControls; ++k)
        set (ids::control (slot, k), k < (int) info.controls.size() ? info.controls[(size_t) k].defaultValue : 0.5f);
    history.commitNow();
}

void AirverbsProcessor::randomise (int slot)
{
    juce::Random rnd;
    auto set = [this] (const juce::String& id, float norm)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            p->endChangeGesture();
        }
    };
    auto randomiseSlot = [&] (int s)
    {
        const auto& reg = EngineRegistry::get();
        const int e = rnd.nextInt (reg.size());
        if (auto* p = apvts.getParameter (ids::engine (s))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) e)); p->endChangeGesture(); }
        const auto& info = reg.info (e);
        for (int k = 0; k < EngineRegistry::maxControls; ++k)
        {
            const float def = k < (int) info.controls.size() ? info.controls[(size_t) k].defaultValue : 0.5f;
            set (ids::control (s, k), def + (rnd.nextFloat() - 0.5f) * 0.6f);
        }
        set (ids::predelay (s), rnd.nextFloat() * rnd.nextFloat() * 0.6f);
        set (ids::pan (s), 0.5f + (rnd.nextFloat() - 0.5f) * 0.5f);
    };

    history.beginAction (slot < 0 ? juce::String ("Randomise constellation") : "Randomise Star " + ids::slotName (slot));
    if (slot >= 0) randomiseSlot (slot);
    else
    {
        for (int s = 0; s < numSlots; ++s) randomiseSlot (s);
        set (ids::topology, (float) rnd.nextInt ((int) Topology::count) / (float) ((int) Topology::count - 1));
        set (ids::morph, rnd.nextFloat());
        set (ids::vecX, 0.2f + rnd.nextFloat() * 0.6f);
        set (ids::vecY, 0.2f + rnd.nextFloat() * 0.6f);
        set (ids::orbitDepth, rnd.nextFloat() * 0.6f);
        set (ids::orbitRate, rnd.nextFloat() * 0.4f);
        set (ids::orbitShape, (float) rnd.nextInt (5) / 4.0f);
    }
    history.commitNow();
}

} // namespace airverbs

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new airverbs::AirverbsProcessor(); }
