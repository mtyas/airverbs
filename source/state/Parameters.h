#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../airwindows/EngineRegistry.h"

namespace airverbs::ids
{
    inline const juce::String mix = "mix", output = "output", input = "input",
        topology = "topology", morph = "morph", vecX = "vec_x", vecY = "vec_y",
        orbitRate = "orbit_rate", orbitDepth = "orbit_depth", orbitShape = "orbit_shape",
        size = "size", decay = "decay", tone = "tone", feedback = "feedback", freeze = "freeze",
        duck = "duck", width = "width", lowCut = "low_cut", highCut = "high_cut";

    inline juce::String slotPrefix (int slot) { return juce::String::charToString ((juce::juce_wchar) ('a' + slot)) + "_"; }
    inline juce::String engine (int s)    { return slotPrefix (s) + "engine"; }
    inline juce::String level (int s)     { return slotPrefix (s) + "level"; }
    inline juce::String pan (int s)       { return slotPrefix (s) + "pan"; }
    inline juce::String slotWidth (int s) { return slotPrefix (s) + "width"; }
    inline juce::String predelay (int s)  { return slotPrefix (s) + "predelay"; }
    inline juce::String mute (int s)      { return slotPrefix (s) + "mute"; }
    inline juce::String control (int s, int k) { return slotPrefix (s) + "p" + juce::String (k + 1); }

    inline juce::String slotName (int s) { return juce::String::charToString ((juce::juce_wchar) ('A' + s)); }
}

namespace airverbs
{
    constexpr int numSlots = 4;
    extern const int defaultEngines[numSlots];

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
