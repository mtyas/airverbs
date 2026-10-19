#include "History.h"

namespace airverbs
{

History::History (juce::AudioProcessor& p) : processor (p)
{
    params = processor.getParameters();
    for (auto* prm : params) prm->addListener (this);
    steps.push_back ({ capture(), "Initial state" });
    startTimerHz (15);
}

History::~History()
{
    stopTimer();
    for (auto* prm : params) prm->removeListener (this);
}

std::vector<float> History::capture() const
{
    std::vector<float> v;
    v.reserve ((size_t) params.size());
    for (auto* prm : params) v.push_back (prm->getValue());
    return v;
}

void History::restore (const std::vector<float>& values)
{
    restoring = true;
    for (int i = 0; i < params.size() && i < (int) values.size(); ++i)
        if (std::abs (params[i]->getValue() - values[(size_t) i]) > 1.0e-6f)
        {
            params[i]->beginChangeGesture();
            params[i]->setValueNotifyingHost (values[(size_t) i]);
            params[i]->endChangeGesture();
        }
    activeGestures = 0;
    changed = false;
    restoring = false;
    if (onChange) onChange();
}

void History::parameterValueChanged (int, float)
{
    if (restoring) return;
    changed = true;
    lastChangeMs = juce::Time::getMillisecondCounter();
}

void History::parameterGestureChanged (int, bool starting)
{
    if (restoring) return;
    activeGestures += starting ? 1 : -1;
    if (activeGestures.load() < 0) activeGestures = 0;
    lastChangeMs = juce::Time::getMillisecondCounter();
}

juce::String History::describeChange (const std::vector<float>& from, const std::vector<float>& to) const
{
    int count = 0, last = -1;
    for (size_t i = 0; i < from.size() && i < to.size(); ++i)
        if (std::abs (from[i] - to[i]) > 1.0e-6f) { ++count; last = (int) i; }
    if (count == 0) return {};
    if (count == 1)
        return params[last]->getName (64) + " " + params[last]->getCurrentValueAsText();
    for (size_t i = 0; i < from.size() && i < to.size(); ++i)
        if (std::abs (from[i] - to[i]) > 1.0e-6f && params[(int) i]->getName (64).endsWith ("Engine"))
            return params[(int) i]->getName (64) + " " + params[(int) i]->getCurrentValueAsText();
    return juce::String (count) + " parameters";
}

void History::commitNow()
{
    auto now = capture();
    auto& top = steps[(size_t) position].values;
    if (now == top) { changed = false; return; }

    juce::String label = pendingLabel.isNotEmpty() ? pendingLabel : describeChange (top, now);
    pendingLabel.clear();
    steps.resize ((size_t) position + 1); // drop the redo branch
    steps.push_back ({ std::move (now), label });
    if (steps.size() > maxSteps) steps.erase (steps.begin());
    position = (int) steps.size() - 1;
    changed = false;
    if (onChange) onChange();
}

void History::beginAction (const juce::String& label)
{
    commitNow();
    pendingLabel = label;
}

void History::timerCallback()
{
    if (! changed.load() || activeGestures.load() > 0) return;
    if (juce::Time::getMillisecondCounter() - lastChangeMs.load() < 350) return;
    commitNow();
}

void History::undo()
{
    commitNow();
    if (! canUndo()) return;
    --position;
    restore (steps[(size_t) position].values);
}

void History::redo()
{
    if (! canRedo()) return;
    ++position;
    restore (steps[(size_t) position].values);
}

void History::jumpTo (int index)
{
    commitNow();
    if (index < 0 || index >= (int) steps.size() || index == position) return;
    position = index;
    restore (steps[(size_t) position].values);
}

juce::StringArray History::getLabels() const
{
    juce::StringArray s;
    for (auto& st : steps) s.add (st.label);
    return s;
}

void History::reset()
{
    steps.clear();
    steps.push_back ({ capture(), "Session loaded" });
    position = 0;
    changed = false;
    pendingLabel.clear();
    if (onChange) onChange();
}

} // namespace airverbs
