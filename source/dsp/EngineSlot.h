#pragma once

#include "DspUtils.h"
#include "../airwindows/EngineRegistry.h"
#include <array>
#include <atomic>
#include <memory>

class AudioEffectX;

namespace airverbs
{

/** Per-star settings handed from the processor to the network every block. */
struct SlotParams
{
    int engine = 0;
    double levelDb = 0.0;
    double pan = 0.0;       // -1..1 balance
    double width = 1.0;     // 0..2 M/S
    double predelayMs = 0.0;
    bool mute = false;
    std::array<float, EngineRegistry::maxControls> controls {};
};

/** Global macros applied on top of each star's native controls. */
struct MacroParams
{
    double size = 0.0, decay = 0.0, tone = 0.0; // bipolar -1..1
};

/**
    One "star" in the constellation: hosts a single Airwindows engine, with a
    pre-delay in front of it and level / pan / width after it.

    Changing algorithm is click-free: a loader thread builds the new engine and hands it
    over through an atomic; the audio thread then crossfades the old engine into the new
    one and hands the old one back for deletion. No allocation happens on the audio thread.
*/
class EngineSlot
{
public:
    EngineSlot();
    ~EngineSlot();

    /** Message thread, audio stopped. Rebuilds the engine synchronously. */
    void prepare (double sampleRate, int maxChunk);

    /** Any thread: ask for a new algorithm. */
    void requestEngine (int index) { requested.store (index); }

    /** Loader thread: builds requested engines and frees retired ones. Returns true if work was done. */
    bool service();

    /** Audio thread. in/out may not alias. */
    void process (const double* inL, const double* inR, double* outL, double* outR, int n,
                  const SlotParams& p, const MacroParams& macros);

    int getActiveEngine() const noexcept { return activeIndex.load(); }
    float getEnergy() const noexcept     { return energy.load(); }

private:
    void applyControls (AudioEffectX& fx, int engineIndex, const SlotParams& p, const MacroParams& m, bool force);

    std::unique_ptr<AudioEffectX> active;
    std::atomic<int> activeIndex { 0 };
    std::atomic<int> requested { 0 };
    std::atomic<AudioEffectX*> incoming { nullptr };
    std::atomic<int> incomingIndex { -1 };
    std::atomic<AudioEffectX*> retired { nullptr };
    int builtIndex = 0;          // loader-side: last engine handed over
    std::atomic<double> loaderSampleRate { 44100.0 };

    AudioEffectX* fading = nullptr;
    int fadingIndex = 0;
    int fadePos = 0, fadeLen = 2048;
    bool forceParams = true;

    std::array<float, 16> lastSent {};
    double sampleRate = 44100.0;
    dsp::StereoDelay predelay;
    dsp::Smoother delaySmooth, gainSmooth, panSmooth, widthSmooth;
    std::vector<double> tmpL, tmpR, preL, preR;
    std::atomic<float> energy { 0.0f };
    double energyState = 0.0;
};

} // namespace airverbs
