#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Constellation.h"
#include "state/Parameters.h"
#include "state/MidiLearn.h"
#include "state/History.h"
#include "presets/PresetManager.h"

namespace airverbs
{

class AirverbsProcessor : public juce::AudioProcessor
{
public:
    AirverbsProcessor();
    ~AirverbsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Swap a star's algorithm and reset its six controls to that engine's defaults: one undo step. */
    void setSlotEngine (int slot, int engineIndex);
    /** Randomise a star (engine + controls) or the whole constellation. */
    void randomise (int slot); // slot < 0 = everything creative

    juce::AudioProcessorValueTreeState apvts;
    History history;
    MidiLearn midiLearn;
    PresetManager presets;
    Constellation network;

    juce::ValueTree uiState { "UI" }; // editor size, camera etc.

    void syncEngineRequests();

private:
    class LoaderThread;
    void readParams (NetworkParams&);

    std::unique_ptr<LoaderThread> loader;
    NetworkParams params;

    struct SlotRefs
    {
        std::atomic<float>* engine; std::atomic<float>* level; std::atomic<float>* pan; std::atomic<float>* width;
        std::atomic<float>* predelay; std::atomic<float>* mute;
        std::array<std::atomic<float>*, EngineRegistry::maxControls> controls;
    };
    std::array<SlotRefs, numSlots> slotRefs;
    std::atomic<float> *mix, *output, *input, *topology, *morph, *vecX, *vecY, *orbitRate, *orbitDepth, *orbitShape,
                       *size, *decay, *tone, *feedback, *freeze, *duck, *width, *lowCut, *highCut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirverbsProcessor)
};

} // namespace airverbs
