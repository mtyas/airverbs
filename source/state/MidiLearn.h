#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <map>

namespace airverbs
{

/**
    MIDI CC learn. Right-click any control -> "MIDI Learn", move a controller, done.
    The audio thread only touches atomics and a lock-free FIFO; parameter updates are
    applied on the message thread so hosts receive them the normal way.
*/
class MidiLearn : private juce::Timer
{
public:
    explicit MidiLearn (juce::AudioProcessor& processor);
    ~MidiLearn() override;

    // message thread
    void startLearning (const juce::String& paramID);
    void cancelLearning();
    juce::String getLearningParam() const;
    bool isLearning() const { return learnIndex.load() >= 0; }
    void clearMapping (const juce::String& paramID);
    void clearAll();
    juce::String describeMapping (const juce::String& paramID) const; // "CC 74 Ch 1" or empty
    int numMappings() const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    std::function<void()> onMappingsChanged;

    /** Message thread: applies queued CC values to parameters (called by the internal timer). */
    void dispatchPending();

    // audio thread
    void processMidi (const juce::MidiBuffer& midi);

private:
    void timerCallback() override;
    int indexOf (const juce::String& paramID) const;

    juce::AudioProcessor& processor;
    juce::Array<juce::AudioProcessorParameter*> params;
    static constexpr int numKeys = 16 * 128;
    std::array<std::atomic<int>, numKeys> table;
    std::atomic<int> learnIndex { -1 };
    std::atomic<bool> mappingsDirty { false };

    struct Event { int param; float value; };
    juce::AbstractFifo fifo { 1024 };
    std::array<Event, 1024> events {};
};

} // namespace airverbs
