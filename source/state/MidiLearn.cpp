#include "MidiLearn.h"

namespace airverbs
{

MidiLearn::MidiLearn (juce::AudioProcessor& p) : processor (p)
{
    params = processor.getParameters();
    for (auto& t : table) t.store (-1);
    startTimerHz (60);
}

MidiLearn::~MidiLearn() { stopTimer(); }

int MidiLearn::indexOf (const juce::String& paramID) const
{
    for (int i = 0; i < params.size(); ++i)
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
            if (wp->paramID == paramID)
                return i;
    return -1;
}

void MidiLearn::startLearning (const juce::String& paramID) { learnIndex.store (indexOf (paramID)); mappingsDirty = true; }
void MidiLearn::cancelLearning()                             { learnIndex.store (-1); mappingsDirty = true; }

juce::String MidiLearn::getLearningParam() const
{
    const int i = learnIndex.load();
    if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
        return wp->paramID;
    return {};
}

void MidiLearn::clearMapping (const juce::String& paramID)
{
    const int idx = indexOf (paramID);
    for (auto& t : table)
        if (t.load() == idx) t.store (-1);
    mappingsDirty = true;
}

void MidiLearn::clearAll()
{
    for (auto& t : table) t.store (-1);
    mappingsDirty = true;
}

int MidiLearn::numMappings() const
{
    int n = 0;
    for (auto& t : table) n += t.load() >= 0 ? 1 : 0;
    return n;
}

juce::String MidiLearn::describeMapping (const juce::String& paramID) const
{
    const int idx = indexOf (paramID);
    if (idx < 0) return {};
    for (int k = 0; k < numKeys; ++k)
        if (table[(size_t) k].load() == idx)
            return "CC " + juce::String (k % 128) + " Ch " + juce::String (k / 128 + 1);
    return {};
}

juce::ValueTree MidiLearn::toValueTree() const
{
    juce::ValueTree v ("MIDI_MAP");
    for (int k = 0; k < numKeys; ++k)
    {
        const int idx = table[(size_t) k].load();
        if (idx < 0) continue;
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[idx]))
        {
            juce::ValueTree m ("MAP");
            m.setProperty ("cc", k % 128, nullptr);
            m.setProperty ("channel", k / 128 + 1, nullptr);
            m.setProperty ("param", wp->paramID, nullptr);
            v.appendChild (m, nullptr);
        }
    }
    return v;
}

void MidiLearn::fromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("MIDI_MAP")) return;
    for (auto& t : table) t.store (-1);
    for (auto m : v)
    {
        const int cc = m.getProperty ("cc"), ch = m.getProperty ("channel");
        const int idx = indexOf (m.getProperty ("param").toString());
        if (idx >= 0 && cc >= 0 && cc < 128 && ch >= 1 && ch <= 16)
            table[(size_t) ((ch - 1) * 128 + cc)].store (idx);
    }
    mappingsDirty = true;
}

void MidiLearn::processMidi (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController()) continue;
        const int key = (msg.getChannel() - 1) * 128 + msg.getControllerNumber();
        if (key < 0 || key >= numKeys) continue;

        const int learning = learnIndex.load();
        if (learning >= 0)
        {
            for (auto& t : table)
                if (t.load() == learning) t.store (-1);
            table[(size_t) key].store (learning);
            learnIndex.store (-1);
            mappingsDirty = true;
        }

        const int idx = table[(size_t) key].load();
        if (idx < 0) continue;
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
            events[(size_t) scope.startIndex1] = { idx, (float) msg.getControllerValue() / 127.0f };
    }
}

void MidiLearn::timerCallback() { dispatchPending(); }

void MidiLearn::dispatchPending()
{
    std::map<int, float> latest;
    const auto scope = fifo.read (fifo.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) { auto& e = events[(size_t) (scope.startIndex1 + i)]; latest[e.param] = e.value; }
    for (int i = 0; i < scope.blockSize2; ++i) { auto& e = events[(size_t) (scope.startIndex2 + i)]; latest[e.param] = e.value; }

    for (auto& [idx, value] : latest)
        if (auto* p = params[idx])
            if (std::abs (p->getValue() - value) > 1.0e-5f)
                p->setValueNotifyingHost (value);

    if (mappingsDirty.exchange (false) && onMappingsChanged)
        onMappingsChanged();
}

} // namespace airverbs
