#include "PresetManager.h"
#include "../state/Parameters.h"

namespace airverbs
{

namespace
{
    enum T { Nebula, Chain, Twin, Bloom, Prism, Dimension, Pulsar };
    enum O { Circle, Figure8, Lissajous, Drift, Pendulum };

    /** Tiny builder so presets read like recipes. Unspecified engine controls get that engine's defaults. */
    struct B
    {
        Preset p;
        B (const char* name, const char* category, const char* description)
        {
            p.name = name; p.category = category; p.description = description; p.author = "mtyas"; p.factory = true;
        }
        B& set (const juce::String& id, float v) { p.values.push_back ({ id, v }); return *this; }
        B& topo (T t, float morph) { return set (ids::topology, (float) t).set (ids::morph, morph); }
        B& vec (float x, float y) { return set (ids::vecX, x).set (ids::vecY, y); }
        B& orbit (O shape, float rate, float depth) { return set (ids::orbitShape, (float) shape).set (ids::orbitRate, rate).set (ids::orbitDepth, depth); }
        B& macros (float size, float decay, float tone) { return set (ids::size, size).set (ids::decay, decay).set (ids::tone, tone); }
        B& mix (float m) { return set (ids::mix, m); }
        B& regen (float r) { return set (ids::feedback, r); }
        B& duck (float d) { return set (ids::duck, d); }
        B& cuts (float lo, float hi) { return set (ids::lowCut, lo).set (ids::highCut, hi); }
        B& width (float w) { return set (ids::width, w); }
        B& star (int s, const char* engine, std::initializer_list<float> controls = {}, float levelDb = 0.0f,
                 float predelayMs = 0.0f, float pan = 0.0f, float width = 1.0f, bool mute = false)
        {
            const auto& reg = EngineRegistry::get();
            const int e = reg.indexOf (engine);
            jassert (e >= 0);
            const auto& info = reg.info (e);
            set (ids::engine (s), (float) e);
            std::vector<float> c (controls);
            for (int k = 0; k < EngineRegistry::maxControls; ++k)
            {
                float v = k < (int) info.controls.size() ? info.controls[(size_t) k].defaultValue : 0.5f;
                if (k < (int) c.size() && c[(size_t) k] >= 0.0f) v = c[(size_t) k];
                set (ids::control (s, k), v);
            }
            return set (ids::level (s), levelDb).set (ids::predelay (s), predelayMs).set (ids::pan (s), pan)
                  .set (ids::slotWidth (s), width).set (ids::mute (s), mute ? 1.0f : 0.0f);
        }
        operator Preset() const { return p; }
    };
    constexpr float _ = -1.0f; // keep the engine default
}

std::vector<Preset> createFactoryPresets()
{
    std::vector<Preset> v;

    // ------------------------------------------------------------------ Rooms & Studios
    v.push_back (B ("Bowie Booth", "Rooms & Studios", "kStation up front, a whisper of kWoodRoom behind. Thickens vocals without washing them out.")
        .star (0, "kStation", { 0.45f, _, 0.45f, 0.6f, 0.85f }).star (1, "kWoodRoom", { 0.4f }, -6.0f, 12.0f)
        .star (2, "Verbity2", { 0.3f, 0.3f, 0.5f }, -12.0f).star (3, "ClearCoat", { 0.3f }, -9.0f, 0.0f, 0.0f, 1.3f)
        .topo (Nebula, 0.1f).vec (0.3f, 0.35f).mix (0.22f).cuts (120.0f, 12000.0f));
    v.push_back (B ("Wooden Stage 1972", "Rooms & Studios", "The kWoodRoom stage with a tape-echo chamber seeding it. Warm and live.")
        .star (0, "kChamberAR", { 0.8f, 0.35f, 0.2f, 0.1f }, -3.0f).star (1, "kWoodRoom", { 0.55f, _, 0.35f, 0.6f, 0.6f })
        .star (2, "kGuitarHall2", { 0.45f }, -6.0f, 18.0f).star (3, "Chamber", { 0.4f, 0.4f, 0.3f, 0.5f }, -8.0f)
        .topo (Bloom, 0.4f).vec (0.6f, 0.4f).mix (0.3f).cuts (90.0f, 11000.0f));
    v.push_back (B ("Small Live Room", "Rooms & Studios", "Four small rooms blended for a believable, non-metallic studio ambience.")
        .star (0, "kStation", { 0.35f }).star (1, "kWoodRoom", { 0.35f }).star (2, "Verbity", { 0.2f, 0.1f, 0.4f })
        .star (3, "PocketVerbs", { 3.0f / 5.0f, 0.3f, 0.0f })
        .topo (Nebula, 0.0f).vec (0.5f, 0.5f).mix (0.25f).cuts (150.0f, 14000.0f));
    v.push_back (B ("Tiled Bathroom", "Rooms & Studios", "PocketVerbs Tiled and a bright ClearCoat bank: hard reflective surfaces.")
        .star (0, "PocketVerbs", { 2.0f / 5.0f, 0.35f, 0.0f }).star (1, "ClearCoat", { 0.6f }).star (2, "kStation", { 0.6f, _, 0.8f })
        .star (3, "BrightAmbience3", { 0.6f, 0.3f, 0.2f }, -6.0f)
        .topo (Nebula, 0.25f).vec (0.4f, 0.4f).mix (0.3f).macros (-0.2f, 0.0f, 0.3f));
    v.push_back (B ("Concrete Stairwell", "Rooms & Studios", "kCyberCity into kChamberAR: concrete, flutter and a long grey tail.")
        .star (0, "kCyberCity", { 0.55f, 0.6f }).star (1, "kChamberAR", { 0.9f, 0.6f, 0.4f, 0.2f })
        .star (2, "Chamber2", { 0.6f, 0.5f, 0.2f }, -6.0f).star (3, "VerbThic", { 0.5f }, -6.0f)
        .topo (Chain, 0.4f).vec (0.4f, 0.2f).mix (0.32f).cuts (180.0f, 9000.0f));

    // ------------------------------------------------------------------ Halls & Cathedrals
    v.push_back (B ("Glass Cathedral", "Halls & Cathedrals", "kCathedral5 and VerbSevens with Galactic shimmer dust in the corners.")
        .star (0, "kCathedral5", { 0.6f, _, 0.4f, 0.5f, 0.7f }).star (1, "VerbSevens", {}, -2.0f, 20.0f)
        .star (2, "Galactic3", { 0.4f, 0.75f, 0.6f, _, 1.0f }, -8.0f, 40.0f, -0.3f, 1.6f).star (3, "kCathedral4", { 0.55f }, -4.0f, 30.0f, 0.3f)
        .topo (Nebula, 0.35f).vec (0.4f, 0.4f).orbit (Circle, 0.05f, 0.25f).mix (0.35f).cuts (140.0f, 13000.0f));
    v.push_back (B ("Recital Hall", "Halls & Cathedrals", "kBeyond's genetic-algorithm hall, with kGuitarHall2 adding midrange body.")
        .star (0, "kBeyond", { 0.5f, _, 0.55f, 0.6f, 0.05f }).star (1, "kGuitarHall2", { 0.45f, _, 0.5f }, -6.0f, 10.0f)
        .star (2, "kRockstar2", { 0.45f, 0.6f, _, 0.4f }, -8.0f).star (3, "VerbSixes", {}, -10.0f)
        .topo (Nebula, 0.15f).vec (0.25f, 0.3f).mix (0.3f).cuts (100.0f, 12000.0f));
    v.push_back (B ("Arena Rockstar", "Halls & Cathedrals", "kRockstar2's conference matrix in front, kCosmos far behind. Huge but focused.")
        .star (0, "kRockstar2", { 0.6f, 0.7f, _, 0.3f }).star (1, "kRockstar", { 0.55f }, -4.0f, 15.0f)
        .star (2, "kCosmos", { 0.5f }, -10.0f, 80.0f).star (3, "kCathedral2", {}, -8.0f, 40.0f)
        .topo (Bloom, 0.5f).vec (0.3f, 0.4f).mix (0.3f).duck (0.3f).cuts (120.0f, 12000.0f));
    v.push_back (B ("Stone Nave", "Halls & Cathedrals", "A chain of cathedrals, each deepening the last. Long, dark and reverent.")
        .star (0, "kCathedral", {}).star (1, "kCathedral3", { 0.4f, 0.6f }).star (2, "kCathedral4", { 0.6f, _, 0.35f })
        .star (3, "kCathedral5", { 0.65f, _, 0.25f, 0.4f, 0.9f })
        .topo (Chain, 0.25f).vec (0.75f, 0.7f).mix (0.32f).macros (0.0f, 0.2f, -0.25f).cuts (80.0f, 8000.0f));
    v.push_back (B ("Midnight Concert Hall", "Halls & Cathedrals", "Prism split: lows to a warm hall, highs to an airy cathedral.")
        .star (0, "kGuitarHall", { 0.5f, 0.5f }).star (1, "kBeyond", { 0.5f }).star (2, "kCathedral5", { 0.55f, _, 0.45f })
        .star (3, "VerbSevens", {})
        .topo (Prism, 0.45f).vec (0.5f, 0.5f).mix (0.3f).cuts (60.0f, 15000.0f));

    // ------------------------------------------------------------------ Plates & Vocals
    v.push_back (B ("Abbey Plates", "Plates & Vocals", "All four kPlates, A to D, like the four plates on the roof. Orbit slowly between them.")
        .star (0, "kPlateA").star (1, "kPlateB").star (2, "kPlateC").star (3, "kPlateD")
        .topo (Nebula, 0.0f).vec (0.5f, 0.5f).orbit (Circle, 0.03f, 0.6f).mix (0.25f).cuts (150.0f, 16000.0f));
    v.push_back (B ("Vocal Plate Deluxe", "Plates & Vocals", "kPlateB with 60 ms predelay, a dusting of kStation and ducking, so the vocal stays clear.")
        .star (0, "kPlateB", { _, 0.45f, 0.8f }, 0.0f, 60.0f).star (1, "kStation", { 0.4f }, -9.0f)
        .star (2, "kPlate240", {}, -8.0f, 40.0f).star (3, "CreamCoat", { 0.5f, 0.4f }, -12.0f, 20.0f)
        .topo (Nebula, 0.0f).vec (0.15f, 0.2f).mix (0.25f).duck (0.45f).cuts (220.0f, 12000.0f));
    v.push_back (B ("Silver Plate 140", "Plates & Vocals", "kPlate140 and kPlate240 in twin chains. Classic, bright and dense.")
        .star (0, "kPlate140", { _, 0.6f }).star (1, "kPlateC", {}, -6.0f).star (2, "kPlate240", { _, 0.55f }).star (3, "kPlateD", {}, -6.0f)
        .topo (Twin, 0.7f).vec (0.5f, 0.3f).mix (0.25f).cuts (180.0f, 14000.0f));
    v.push_back (B ("Ballad Bloom", "Plates & Vocals", "Pulsar: consonants hit a short plate, sustained notes bloom into a hall.")
        .star (0, "kPlateA", { _, 0.3f }).star (1, "kStation", { 0.3f }).star (2, "kCathedral4", { 0.6f }).star (3, "Galactic3", { 0.35f, 0.6f })
        .topo (Pulsar, 0.55f).vec (0.5f, 0.6f).mix (0.3f).duck (0.25f).cuts (160.0f, 12000.0f));
    v.push_back (B ("Creamy Backing Vox", "Plates & Vocals", "CreamCoat and kPlateD, slightly wide. Glues stacked harmonies together.")
        .star (0, "CreamCoat", { 0.4f, 0.5f, 1.0f, 0.1f }).star (1, "kPlateD").star (2, "Verbity2", { 0.4f, 0.4f, 0.4f }, -6.0f)
        .star (3, "ClearCoat", { 0.5f }, -9.0f, 0.0f, 0.0f, 1.6f)
        .topo (Nebula, 0.15f).vec (0.4f, 0.35f).mix (0.3f).width (1.3f).cuts (200.0f, 13000.0f));

    // ------------------------------------------------------------------ Drums & Percussion
    v.push_back (B ("Gated Brightness", "Drums & Percussion", "BrightAmbience3 in front of a ducked room: the 80s, done the Airwindows way.")
        .star (0, "BrightAmbience3", { 0.8f, 0.4f, 0.3f }).star (1, "BrightAmbience2", { 0.3f, 0.3f, 0.2f })
        .star (2, "kStation", { 0.3f }, -6.0f).star (3, "PocketVerbs", { 0.6f, 0.4f, 0.7f })
        .topo (Nebula, 0.0f).vec (0.35f, 0.4f).mix (0.3f).cuts (200.0f, 15000.0f));
    v.push_back (B ("Snare Chamber", "Drums & Percussion", "Pulsar sends the snare crack to a bright chamber, the ring to a plate.")
        .star (0, "kChamberAR", { 0.9f, 0.3f, 0.1f, 0.3f }).star (1, "ClearCoat", { 0.7f }).star (2, "kPlateC", { _, 0.5f })
        .star (3, "Chamber", { 0.3f, 0.3f, 0.5f, 0.3f })
        .topo (Pulsar, 0.8f).vec (0.3f, 0.5f).mix (0.28f).cuts (250.0f, 14000.0f));
    v.push_back (B ("Overhead Air", "Drums & Percussion", "Dimension: the kit's centre stays tight while the sides open into a big room.")
        .star (0, "kStation", { 0.3f }).star (1, "ClearCoat", { 0.4f }).star (2, "kBeyond", { 0.5f }).star (3, "kRockstar2", { 0.5f })
        .topo (Dimension, 0.6f).vec (0.5f, 0.6f).mix (0.25f).cuts (300.0f, 16000.0f));
    v.push_back (B ("Kick Tunnel", "Drums & Percussion", "Prism with the low band in a dark chamber and the clicks in tiny rooms.")
        .star (0, "Chamber2", { 0.5f, 0.4f, 0.4f }).star (1, "kCyberCity", { 0.4f }).star (2, "kStation", { 0.2f }).star (3, "BrightAmbience3")
        .topo (Prism, 0.25f).vec (0.3f, 0.5f).mix (0.22f).cuts (40.0f, 10000.0f));

    // ------------------------------------------------------------------ Ambient & Cosmic
    v.push_back (B ("Galactic Drift", "Ambient & Cosmic", "Three Galactics and kCosmos, with the listener drifting at random between them.")
        .star (0, "Galactic", { 0.3f, 0.6f, 0.6f }).star (1, "Galactic2", { 0.5f, 0.7f, 0.3f }).star (2, "Galactic3", { 0.25f, 0.5f, 0.7f })
        .star (3, "kCosmos", { 0.7f })
        .topo (Nebula, 0.5f).vec (0.5f, 0.5f).orbit (Drift, 0.08f, 0.9f).mix (0.5f).regen (0.25f).cuts (100.0f, 12000.0f));
    v.push_back (B ("Infinite Nebula", "Ambient & Cosmic", "Infinity2 and MatrixVerb crossfeeding. Hit Freeze to hold a chord forever.")
        .star (0, "Infinity2", { 0.6f, 0.7f, 0.3f }).star (1, "MatrixVerb", { 0.7f, 0.2f, 0.2f, 0.3f, 0.8f }).star (2, "kCosmos", { 0.75f })
        .star (3, "Galactic3", { 0.2f, 0.4f, 0.8f })
        .topo (Nebula, 0.7f).vec (0.5f, 0.5f).orbit (Lissajous, 0.04f, 0.5f).mix (0.55f).regen (0.35f).cuts (120.0f, 9000.0f));
    v.push_back (B ("Event Horizon", "Ambient & Cosmic", "A chain that ends in kCosmos, with the outer loop nearly at self-oscillation.")
        .star (0, "MV2", { 0.6f, 0.4f, 0.6f }).star (1, "Galactic3", { 0.3f, 0.5f, 0.6f }).star (2, "VerbSevens").star (3, "kCosmos", { 0.8f })
        .topo (Chain, 0.2f).vec (0.85f, 0.85f).mix (0.6f).regen (0.6f).macros (0.3f, 0.3f, -0.2f).cuts (150.0f, 8000.0f));
    v.push_back (B ("Aurora Pad", "Ambient & Cosmic", "Wide, slow and luminous. Figure-8 orbit through four lush engines.")
        .star (0, "Galactic3", { 0.35f, 0.8f }, 0.0f, 0.0f, -0.5f, 1.8f).star (1, "kCathedral5", { 0.7f, _, 0.6f }, 0.0f, 50.0f, 0.5f)
        .star (2, "CloudCoat", { 0.6f, 0.7f }, -4.0f).star (3, "Infinity", { 0.8f, 0.2f, 0.7f }, -4.0f)
        .topo (Nebula, 0.4f).vec (0.5f, 0.5f).orbit (Figure8, 0.06f, 0.8f).mix (0.5f).width (1.5f).cuts (140.0f, 14000.0f));
    v.push_back (B ("Starchild Echoes", "Ambient & Cosmic", "StarChild2's grainy echoes blooming into Galactic. Very much a vibe.")
        .star (0, "StarChild2", { 0.8f, 0.6f }).star (1, "StarChild", { 0.7f, 0.4f }).star (2, "Galactic", { 0.4f, 0.5f, 0.5f }).star (3, "VerbSixes")
        .topo (Bloom, 0.3f).vec (0.6f, 0.6f).mix (0.45f).regen (0.15f).cuts (150.0f, 10000.0f));

    // ------------------------------------------------------------------ Creative Topologies
    v.push_back (B ("Reverb of Reverbs", "Creative Topologies", "Nebula at full crossfeed: every star feeds every other. A blooming, living space.")
        .star (0, "kStation").star (1, "kPlateB").star (2, "kCathedral5").star (3, "Galactic3")
        .topo (Nebula, 1.0f).vec (0.5f, 0.5f).mix (0.4f).cuts (120.0f, 12000.0f));
    v.push_back (B ("Cascade Explorer", "Creative Topologies", "A four-stage chain. Drag the listener from A to D to go deeper into the cascade.")
        .star (0, "kStation", { 0.4f }).star (1, "kPlateA").star (2, "kBeyond").star (3, "kCosmos")
        .topo (Chain, 0.3f).vec (0.0f, 0.0f).orbit (Lissajous, 0.03f, 0.0f).mix (0.35f).cuts (120.0f, 12000.0f));
    v.push_back (B ("Left Brain Right Brain", "Creative Topologies", "Twin chains fully split: the left channel lives in plates, the right in halls.")
        .star (0, "kPlateA").star (1, "kPlate140").star (2, "kGuitarHall2").star (3, "kRockstar2")
        .topo (Twin, 1.0f).vec (0.5f, 0.5f).mix (0.35f).width (1.4f));
    v.push_back (B ("Spectral Prism", "Creative Topologies", "Four different worlds per band: dark chamber, room, plate and bright ambience.")
        .star (0, "Chamber", { 0.6f, 0.6f, 0.2f, 0.6f }).star (1, "kWoodRoom").star (2, "kPlateC").star (3, "ClearCoat", { 0.8f })
        .topo (Prism, 0.5f).vec (0.5f, 0.5f).orbit (Circle, 0.15f, 0.7f).mix (0.35f));
    v.push_back (B ("Mid Dry, Sides Cosmic", "Creative Topologies", "Dimension: the centre gets a tight room, the sides get infinite space.")
        .star (0, "kStation", { 0.3f }).star (1, "VerbTiny", { 0.6f }).star (2, "kCosmos", { 0.7f }).star (3, "Galactic3", { 0.3f })
        .topo (Dimension, 0.8f).vec (0.5f, 0.7f).mix (0.35f).width (1.3f));
    v.push_back (B ("Transient Splitter", "Creative Topologies", "Pulsar at high sensitivity: attacks scatter into gated ambience, sustain into a hall.")
        .star (0, "BrightAmbience3", { 0.9f, 0.5f, 0.3f }).star (1, "ClearCoat", { 0.9f }).star (2, "kCathedral5", { 0.6f }).star (3, "VerbSevens")
        .topo (Pulsar, 0.9f).vec (0.5f, 0.5f).mix (0.35f));
    v.push_back (B ("Pendulum Swing", "Creative Topologies", "The listener swings from a small room to a cathedral and back, once every 4 seconds.")
        .star (0, "kStation").star (1, "kCathedral5", { 0.65f }).star (2, "kStation").star (3, "kCathedral5", { 0.65f })
        .topo (Nebula, 0.1f).vec (0.5f, 0.5f).orbit (Pendulum, 0.25f, 1.0f).mix (0.35f));

    // ------------------------------------------------------------------ Sound Design
    v.push_back (B ("Alien Mothership", "Sound Design", "kAlienSpaceship blooming into MV2 and Melt. Unreal, but somehow real.")
        .star (0, "kAlienSpaceship", { 0.6f, 0.3f, 0.6f }).star (1, "MV2", { 0.8f, 0.3f, 0.7f }).star (2, "Melt", { 0.6f, 0.8f })
        .star (3, "CloudCoat", { 0.9f, 0.7f })
        .topo (Bloom, 0.3f).vec (0.6f, 0.6f).orbit (Lissajous, 0.12f, 0.6f).mix (0.5f).regen (0.3f));
    v.push_back (B ("Midiverb Memories", "Sound Design", "MV and MV2, the Alesis Midiverb homages, crunchy and lo-fi in parallel.")
        .star (0, "MV", { 0.5f, 0.4f, 0.5f }).star (1, "MV2", { 0.6f, 0.6f, 0.4f }).star (2, "CrunchCoat", { 0.6f, 0.4f, 0.4f })
        .star (3, "StarChild", { 0.6f, 0.8f })
        .topo (Nebula, 0.2f).vec (0.4f, 0.4f).mix (0.35f).cuts (200.0f, 7000.0f));
    v.push_back (B ("Evil Clouds", "Sound Design", "CloudCoat's darker selections in a feedback ring. Menacing blur.")
        .star (0, "CloudCoat", { 0.2f, 0.8f }).star (1, "CloudCoat", { 0.7f, 0.8f }).star (2, "NonlinearSpace", { 0.3f, 0.7f, 0.3f, 0.6f, 0.7f })
        .star (3, "Infinity", { 0.3f, 0.3f, 0.8f })
        .topo (Chain, 0.4f).vec (0.6f, 0.7f).mix (0.5f).regen (0.45f).macros (0.0f, 0.2f, -0.4f));
    v.push_back (B ("Frozen Lake", "Sound Design", "Play something, then press Freeze. The space hangs suspended while you play over it.")
        .star (0, "Infinity2", { 0.8f, 0.8f, 0.2f }).star (1, "Galactic3", { 0.2f, 0.7f, 0.7f }).star (2, "kCosmos").star (3, "MatrixVerb", { 0.9f, 0.1f, 0.3f, 0.2f, 0.9f })
        .topo (Nebula, 0.6f).vec (0.5f, 0.5f).orbit (Circle, 0.02f, 0.4f).mix (0.5f).regen (0.3f).cuts (150.0f, 11000.0f));
    v.push_back (B ("Melting Clock", "Sound Design", "Melt and Chamber2 glitch blur in a chain, wobbling through time.")
        .star (0, "Melt", { 0.8f, 0.6f }).star (1, "Chamber2", { 0.7f, 0.6f, 0.8f }).star (2, "MV2", { 0.7f, 0.3f, 0.6f }).star (3, "Melt", { 0.4f, 0.9f })
        .topo (Chain, 0.5f).vec (0.7f, 0.5f).orbit (Drift, 0.3f, 0.5f).mix (0.45f));

    // ------------------------------------------------------------------ Mix Utility
    v.push_back (B ("Subtle Glue", "Mix Utility", "A barely-there blend of small rooms that puts a dry mix in one space.")
        .star (0, "kStation", { 0.25f }).star (1, "Verbity2", { 0.2f, 0.2f, 0.5f }).star (2, "kWoodRoom", { 0.25f }).star (3, "Ultralight2", { 0.5f })
        .topo (Nebula, 0.0f).vec (0.5f, 0.5f).mix (0.15f).cuts (200.0f, 12000.0f));
    v.push_back (B ("Send: Medium Hall", "Mix Utility", "A 100% wet hall for an aux send. Low cut keeps the mix clean.")
        .star (0, "kBeyond", { 0.5f }).star (1, "kGuitarHall2", { 0.5f }).star (2, "kCathedral4", { 0.45f }).star (3, "VerbSevens", {}, -6.0f)
        .topo (Nebula, 0.1f).vec (0.4f, 0.4f).mix (1.0f).cuts (250.0f, 11000.0f));
    v.push_back (B ("Send: Short Plate", "Mix Utility", "A 100% wet bright plate for an aux send.")
        .star (0, "kPlateA", { _, 0.3f }).star (1, "kPlateB", { _, 0.3f }).star (2, "kPlate140", { _, 0.35f }).star (3, "kPlateD", { _, 0.3f })
        .topo (Nebula, 0.0f).vec (0.5f, 0.5f).mix (1.0f).cuts (300.0f, 14000.0f));
    v.push_back (B ("Ducked Ambience", "Mix Utility", "Heavy ducking: the reverb only appears between phrases. Great on dialogue and leads.")
        .star (0, "kStation").star (1, "kWoodRoom").star (2, "kBeyond", { 0.5f }).star (3, "ClearCoat", { 0.4f })
        .topo (Nebula, 0.1f).vec (0.5f, 0.5f).mix (0.35f).duck (0.8f).cuts (200.0f, 12000.0f));
    v.push_back (B ("Ultralight Game Room", "Mix Utility", "The lowest-CPU engines: Ultralight, Ultralight2 and VerbTiny.")
        .star (0, "Ultralight").star (1, "Ultralight2").star (2, "VerbTiny", { 0.5f }).star (3, "Reverb", { 0.4f })
        .topo (Nebula, 0.0f).vec (0.5f, 0.5f).mix (0.3f));

    return v;
}

} // namespace airverbs
