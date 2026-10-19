#pragma once

#include "EngineSlot.h"
#include <array>
#include <atomic>
#include <mutex>
#include <random>

namespace airverbs
{

enum class Topology
{
    Nebula,     // four stars in parallel, cross-feeding each other
    Chain,      // A -> B -> C -> D, vector pad picks the depth you hear
    Twin,       // (A -> B) || (C -> D), optional L/R split
    Bloom,      // A is the early field that seeds B, C, D
    Prism,      // 4-band crossover: lows to A ... highs to D
    Dimension,  // mid feeds A + B, side feeds C + D
    Pulsar,     // transients go to A + B, sustain goes to C + D
    count
};

enum class OrbitShape { Circle, Figure8, Lissajous, Drift, Pendulum, count };

inline const char* topologyName (int t)
{
    static const char* names[] = { "Nebula", "Chain", "Twin", "Bloom", "Prism", "Dimension", "Pulsar" };
    return names[t < 0 || t >= (int) Topology::count ? 0 : t];
}
inline const char* topologyMorphName (int t)
{
    static const char* names[] = { "Crossfeed", "Thru", "Split", "Seed", "Crossover", "Side Gain", "Sensitivity" };
    return names[t < 0 || t >= (int) Topology::count ? 0 : t];
}
inline const char* topologyDescription (int t)
{
    static const char* d[] = {
        "All four stars in parallel. Crossfeed lets each star feed the others, a reverb of reverbs.",
        "A into B into C into D. The vector pad chooses how deep into the cascade you listen.",
        "Two parallel chains, A to B and C to D. Split sends the left channel to one chain and the right to the other.",
        "Star A builds the early field and seeds B, C and D. Seed mixes in the dry signal too.",
        "A 4-band crossover: lows to A, low mids to B, high mids to C, highs to D. Crossover moves the bands.",
        "Mid feeds A and B, side feeds C and D, whose returns land only in the sides.",
        "Transient detector: attacks go to A and B, sustain goes to C and D. Sensitivity sets the split.",
    };
    return d[t < 0 || t >= (int) Topology::count ? 0 : t];
}

struct NetworkParams
{
    std::array<SlotParams, 4> slots;
    MacroParams macros;
    Topology topology = Topology::Nebula;
    double morph = 0.5;
    double vecX = 0.5, vecY = 0.5;
    double orbitRate = 0.1, orbitDepth = 0.0;
    OrbitShape orbitShape = OrbitShape::Circle;
    double feedback = 0.0;
    bool freeze = false;
    double duck = 0.0;
    double width = 1.0;
    double lowCut = 20.0, highCut = 20000.0;
    double mix = 0.35;
    double outputDb = 0.0;
    double inputDb = 0.0;
};

/** Live state the GUI reads (atomics, written once per chunk). */
struct NetworkTelemetry
{
    std::atomic<float> posX { 0.5f }, posY { 0.5f };
    std::array<std::atomic<float>, 4> weight {};
    std::array<std::atomic<float>, 4> energy {};
    std::atomic<float> inputLevel { 0.0f }, wetLevel { 0.0f }, transient { 0.0f }, loopGain { 0.0f };
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };
};

/**
    The Airverbs network: four Airwindows engines ("stars") wired together by one of
    seven topologies, morphed by a vector pad that can orbit on its own, and wrapped in a
    regenerative outer feedback loop with freeze.
*/
class Constellation
{
public:
    static constexpr int chunk = 32; // fixed internal block: keeps the feedback loop's timing host-independent

    void prepare (double sampleRate, int maxBlock);
    void process (float* const* channels, int numChannels, int numSamples, const NetworkParams& p);

    EngineSlot& slot (int i) { return slots[(size_t) i]; }
    bool serviceLoaders();   // background thread
    NetworkTelemetry telemetry;

private:
    void processChunk (const double* inL, const double* inR, double* outL, double* outR, int n, const NetworkParams& p);
    void updateOrbit (const NetworkParams& p, int n);
    void computeWeights (double x, double y, std::array<double, 4>& w) const;

    double sampleRate = 44100.0;
    std::array<EngineSlot, 4> slots;
    std::mutex loaderMutex; // serialises engine building between the loader thread and prepare/setState

    // scratch (chunk sized)
    std::array<std::array<double, chunk>, 4> slotInL {}, slotInR {}, slotOutL {}, slotOutR {};
    std::array<double, chunk> netL {}, netR {}, wetL {}, wetR {}, dryL {}, dryR {}, fbL {}, fbR {};
    std::array<double, 4> prevEnergyL {}, prevEnergyR {};
    std::array<std::array<double, chunk>, 4> prevOutL {}, prevOutR {};

    // prism crossovers (stereo)
    std::array<dsp::LR4Split, 3> xoverL, xoverR;
    double lastCrossover = -1.0;

    // pulsar detector
    dsp::EnvelopeFollower fastEnv, slowEnv, duckEnv;
    dsp::Smoother transientSmooth;

    // feedback loop
    dsp::Svf fbHpL, fbHpR, fbLpL, fbLpR;
    double fbGainState = 0.0, loopRms = 0.0, freezeTarget = 0.0, crossfeedRms = 0.0, networkInputEnv = 0.0;
    bool wasFrozen = false;
    dsp::Smoother inputGate;

    // output stage
    dsp::Svf lowCutL[2], lowCutR[2], highCutL[2], highCutR[2];
    double lastLowCut = -1.0, lastHighCut = -1.0;
    dsp::Smoother mixSmooth, outSmooth, inSmooth, widthSmooth, morphSmooth, duckSmooth;
    std::array<dsp::Smoother, 4> weightSmooth, muteSmooth;
    static constexpr int fbRingSize = 128;
    std::array<double, fbRingSize> fbRingL {}, fbRingR {};
    int fbWrite = 0;

    // orbit
    double orbitPhase = 0.0, curX = 0.5, curY = 0.5;
    double driftX = 0.0, driftY = 0.0, driftTX = 0.0, driftTY = 0.0, driftClock = 0.0;
    std::minstd_rand rng { 0xA1F };

    double peakL = 0.0, peakR = 0.0;
};

} // namespace airverbs
