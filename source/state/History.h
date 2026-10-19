#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace airverbs
{

/**
    Snapshot-based undo / redo across every parameter.

    Edits are grouped automatically: a step is committed once no gesture is in progress and
    the parameters have been still for a moment, so a knob drag, an engine swap that resets
    six controls, or a whole preset load each become exactly one undo step.
*/
class History : private juce::Timer,
                private juce::AudioProcessorParameter::Listener
{
public:
    explicit History (juce::AudioProcessor&);
    ~History() override;

    void undo();
    void redo();
    bool canUndo() const { return position > 0; }
    bool canRedo() const { return position < (int) steps.size() - 1; }
    juce::String getUndoLabel() const { return canUndo() ? steps[(size_t) position].label : juce::String(); }
    juce::String getRedoLabel() const { return canRedo() ? steps[(size_t) position + 1].label : juce::String(); }
    juce::StringArray getLabels() const;
    int getPosition() const { return position; }
    void jumpTo (int index);

    /** Names the next committed step (e.g. "Load preset Glass Cathedral"). Commits pending edits first. */
    void beginAction (const juce::String& label);
    /** Commits any pending edits under the current label right now. */
    void commitNow();
    /** Forget everything and start fresh from the current state (after loading a session). */
    void reset();

    std::function<void()> onChange;

private:
    struct Step { std::vector<float> values; juce::String label; };

    void timerCallback() override;
    void parameterValueChanged (int index, float) override;
    void parameterGestureChanged (int, bool starting) override;
    std::vector<float> capture() const;
    void restore (const std::vector<float>&);
    juce::String describeChange (const std::vector<float>& from, const std::vector<float>& to) const;

    juce::AudioProcessor& processor;
    juce::Array<juce::AudioProcessorParameter*> params;
    std::vector<Step> steps;
    int position = 0;
    juce::String pendingLabel;
    std::atomic<int> activeGestures { 0 };
    std::atomic<juce::uint32> lastChangeMs { 0 };
    std::atomic<bool> changed { false };
    bool restoring = false;
    static constexpr size_t maxSteps = 256;
};

} // namespace airverbs
