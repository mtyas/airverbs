// Processor-level checks: every factory preset renders cleanly, state survives a save/load
// round trip, MIDI learn maps a CC, and undo/redo restores parameters.
#include "../source/PluginProcessor.h"

using namespace airverbs;

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::cout << "  FAIL: " << msg << std::endl; } } while (0)

static void pump (int ms) { juce::Thread::sleep (ms); } // engines are built by the loader thread

struct Render { double rms = 0, peak = 0; bool finite = true; };

static Render render (AirverbsProcessor& p, double seconds, double sr, int block, juce::MidiBuffer* midi = nullptr)
{
    Render r;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer empty;
    juce::Random rnd (1);
    const int total = (int) (seconds * sr);
    double sq = 0; long n = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buf.setSample (ch, i, ((pos + i) % (int) sr) < (int) (0.25 * sr) ? (rnd.nextFloat() * 2.0f - 1.0f) * 0.25f : 0.0f);
        p.processBlock (buf, midi != nullptr && pos == 0 ? *midi : empty);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float s = buf.getSample (ch, i);
                if (! std::isfinite (s)) r.finite = false;
                r.peak = std::max (r.peak, (double) std::abs (s));
                sq += (double) s * s; ++n;
            }
    }
    r.rms = std::sqrt (sq / (double) std::max (1L, n));
    return r;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<AirverbsProcessor> proc (dynamic_cast<AirverbsProcessor*> (createPluginFilter()));
    auto& p = *proc;
    const double sr = 48000.0;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);

    std::cout << "Factory presets (" << p.presets.getAll().size() << "):" << std::endl;
    for (int i = 0; i < (int) p.presets.getAll().size(); ++i)
    {
        const auto& pr = p.presets.getAll()[(size_t) i];
        if (! pr.factory) continue;
        p.presets.load (i);
        pump (40); // let the loader thread build the engines
        const auto r = render (p, 4.0, sr, 512);
        std::cout << "  " << juce::String (pr.name).paddedRight (' ', 26) << juce::String (pr.category).paddedRight (' ', 22)
                  << "rms " << juce::String (juce::Decibels::gainToDecibels (r.rms), 1) << " dB  peak "
                  << juce::String (juce::Decibels::gainToDecibels (r.peak), 1) << " dB" << std::endl;
        CHECK (r.finite, pr.name << " produced NaN/Inf");
        CHECK (r.peak < 4.0, pr.name << " too hot");
        CHECK (r.rms > 1.0e-4, pr.name << " silent");
        for (int s = 0; s < numSlots; ++s)
        {
            const int want = (int) p.apvts.getRawParameterValue (ids::engine (s))->load();
            CHECK (p.network.slot (s).getActiveEngine() == want, pr.name << " star " << s << " engine not active");
        }
    }

    std::cout << "State round trip:" << std::endl;
    p.presets.load (5);
    pump (40);
    p.midiLearn.startLearning (ids::mix);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 0);
    render (p, 0.05, sr, 512, &midi);
    p.midiLearn.dispatchPending();
    CHECK (p.midiLearn.describeMapping (ids::mix) == "CC 74 Ch 1", "MIDI learn did not map CC 74");
    CHECK (std::abs (p.apvts.getRawParameterValue (ids::mix)->load() - 100.0f / 127.0f) < 0.01f, "learned CC did not move Mix");

    juce::MemoryBlock state;
    p.getStateInformation (state);
    std::unique_ptr<AirverbsProcessor> proc2 (dynamic_cast<AirverbsProcessor*> (createPluginFilter()));
    proc2->setPlayConfigDetails (2, 2, sr, 512);
    proc2->prepareToPlay (sr, 512);
    proc2->setStateInformation (state.getData(), (int) state.getSize());
    render (*proc2, 0.1, sr, 512); // engines are handed to the audio thread on the next block
    bool same = true;
    for (int i = 0; i < p.getParameters().size(); ++i)
        same &= std::abs (p.getParameters()[i]->getValue() - proc2->getParameters()[i]->getValue()) < 1.0e-5f;
    CHECK (same, "parameters differ after state round trip");
    CHECK (proc2->midiLearn.describeMapping (ids::mix) == "CC 74 Ch 1", "MIDI map lost in state");
    CHECK (proc2->presets.getCurrentName() == p.presets.getCurrentName(), "preset name lost in state");
    for (int s = 0; s < numSlots; ++s)
        CHECK (proc2->network.slot (s).getActiveEngine() == (int) proc2->apvts.getRawParameterValue (ids::engine (s))->load(),
               "engine not rebuilt after setState (star " << s << ")");

    std::cout << "Undo / redo:" << std::endl;
    p.history.reset();
    auto* decay = p.apvts.getParameter (ids::decay);
    const float before = decay->getValue();
    decay->beginChangeGesture(); decay->setValueNotifyingHost (0.9f); decay->endChangeGesture();
    p.history.commitNow();
    p.setSlotEngine (2, EngineRegistry::get().indexOf ("kPlateC"));
    CHECK ((int) p.apvts.getRawParameterValue (ids::engine (2))->load() == EngineRegistry::get().indexOf ("kPlateC"), "setSlotEngine failed");
    p.history.undo();
    CHECK ((int) p.apvts.getRawParameterValue (ids::engine (2))->load() != EngineRegistry::get().indexOf ("kPlateC"), "undo engine swap failed");
    p.history.undo();
    CHECK (std::abs (decay->getValue() - before) < 1.0e-5f, "undo of decay failed");
    p.history.redo();
    CHECK (std::abs (decay->getValue() - 0.9f) < 1.0e-5f, "redo of decay failed");
    std::cout << "  history: " << p.history.getLabels().joinIntoString (" | ") << std::endl;

    std::cout << "User preset save / load / delete:" << std::endl;
    const int before2 = (int) p.presets.getAll().size();
    CHECK (p.presets.saveUser ("Airverbs Test Preset", "Test", "temporary"), "save failed");
    CHECK ((int) p.presets.getAll().size() == before2 + 1, "saved preset not listed");
    const int idx = p.presets.getCurrentIndex();
    CHECK (idx >= 0 && ! p.presets.getAll()[(size_t) idx].factory, "saved preset not current");
    CHECK (p.presets.load (idx), "load user preset failed");
    CHECK (p.presets.deleteUser (idx), "delete failed");
    PresetManager::getUserDirectory().getChildFile ("Test").deleteRecursively();

    proc2.reset();
    proc.reset();
    std::cout << (failures == 0 ? "\nALL PASSED" : "\nFAILURES: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
