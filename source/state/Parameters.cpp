#include "Parameters.h"
#include "../dsp/Constellation.h"

namespace airverbs
{

const int defaultEngines[numSlots] = {
    EngineRegistry::get().indexOf ("kStation"),
    EngineRegistry::get().indexOf ("kPlateB"),
    EngineRegistry::get().indexOf ("kCathedral5"),
    EngineRegistry::get().indexOf ("Galactic3"),
};

using namespace juce;

static NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

static String pct (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; }
static String bipolarPct (float v, int) { const int p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; }
static String hz (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
static String db (float v, int) { return v <= -47.9f ? String ("-inf dB") : String (v, 1) + " dB"; }

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int v = 1;
    auto pctAttr  = AudioParameterFloatAttributes().withStringFromValueFunction (pct);
    auto biAttr   = AudioParameterFloatAttributes().withStringFromValueFunction (bipolarPct);
    auto hzAttr   = AudioParameterFloatAttributes().withStringFromValueFunction (hz);
    auto dbAttr   = AudioParameterFloatAttributes().withStringFromValueFunction (db);

    auto global = std::make_unique<AudioProcessorParameterGroup> ("global", "Global", " | ");
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, v }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, dbAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, v }, "Input", NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, dbAttr));

    StringArray topologies;
    for (int t = 0; t < (int) Topology::count; ++t) topologies.add (topologyName (t));
    global->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::topology, v }, "Topology", topologies, 0));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::morph, v }, "Morph", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::vecX, v }, "Vector X", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::vecY, v }, "Vector Y", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::orbitRate, v }, "Orbit Rate", skewed (0.01f, 8.0f, 0.4f), 0.12f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return x < 1.0f ? String (x, 2) + " Hz" : String (x, 1) + " Hz"; })));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::orbitDepth, v }, "Orbit Depth", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pctAttr));
    global->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::orbitShape, v }, "Orbit Shape",
                                                              StringArray { "Circle", "Figure 8", "Lissajous", "Drift", "Pendulum" }, 0));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::size, v }, "Size", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, biAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::decay, v }, "Decay", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, biAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::tone, v }, "Tone", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, biAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::feedback, v }, "Regen", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pctAttr));
    global->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::freeze, v }, "Freeze", false));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::duck, v }, "Duck", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::width, v }, "Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pctAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::lowCut, v }, "Low Cut", skewed (20.0f, 1200.0f, 150.0f), 20.0f, hzAttr));
    global->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::highCut, v }, "High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, hzAttr));
    layout.add (std::move (global));

    const auto engineNames = [] { StringArray s; for (auto& n : EngineRegistry::get().engineNames()) s.add (n); return s; }();
    for (int s = 0; s < numSlots; ++s)
    {
        const auto star = "Star " + ids::slotName (s);
        auto g = std::make_unique<AudioProcessorParameterGroup> (ids::slotPrefix (s) + "group", star, " | ");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::engine (s), v }, star + " Engine", engineNames, defaultEngines[s]));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::level (s), v }, star + " Level", NormalisableRange<float> (-48.0f, 6.0f, 0.01f, 2.5f), 0.0f, dbAttr));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::pan (s), v }, star + " Pan", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) {
                const int p = roundToInt (x * 100.0f);
                return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p)); })));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::slotWidth (s), v }, star + " Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pctAttr));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::predelay (s), v }, star + " Predelay", skewed (0.0f, 500.0f, 60.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, x < 10.0f ? 1 : 0) + " ms"; })));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::mute (s), v }, star + " Mute", false));
        const auto& info = EngineRegistry::get().info (defaultEngines[s]);
        for (int k = 0; k < EngineRegistry::maxControls; ++k)
        {
            const float def = k < (int) info.controls.size() ? info.controls[(size_t) k].defaultValue : 0.5f;
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::control (s, k), v }, star + " P" + String (k + 1),
                                                                NormalisableRange<float> (0.0f, 1.0f), def, pctAttr));
        }
        layout.add (std::move (g));
    }
    return layout;
}

} // namespace airverbs
