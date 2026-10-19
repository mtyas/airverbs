#include "EngineSlot.h"
#include "audioeffectx.h"

namespace airverbs
{

EngineSlot::EngineSlot() = default;

EngineSlot::~EngineSlot()
{
    delete incoming.exchange (nullptr);
    delete retired.exchange (nullptr);
    delete fading;
}

void EngineSlot::prepare (double sr, int maxChunk)
{
    sampleRate = sr;
    loaderSampleRate.store (sr);
    const int want = EngineRegistry::get().clampIndex (requested.load());
    delete incoming.exchange (nullptr);
    delete retired.exchange (nullptr);
    delete fading;
    fading = nullptr;
    active = EngineRegistry::get().createEngine (want, sr);
    activeIndex.store (want);
    builtIndex = want;
    forceParams = true;

    fadeLen = (int) (0.06 * sr);
    predelay.prepare ((int) (0.52 * sr));
    tmpL.assign ((size_t) maxChunk, 0.0); tmpR = tmpL; preL = tmpL; preR = tmpL;
    delaySmooth.reset (sr, 80.0, 0.0);
    gainSmooth.reset (sr, 20.0, 0.0);
    panSmooth.reset (sr, 20.0, 0.0);
    widthSmooth.reset (sr, 20.0, 1.0);
    energyState = 0.0;
}

bool EngineSlot::service()
{
    bool didWork = false;
    if (auto* r = retired.exchange (nullptr)) { delete r; didWork = true; }

    const int want = EngineRegistry::get().clampIndex (requested.load());
    if (want != builtIndex)
    {
        auto fx = EngineRegistry::get().createEngine (want, loaderSampleRate.load());
        incomingIndex.store (want);
        // If the audio thread never picked up a previous engine, we still own it.
        delete incoming.exchange (fx.release());
        builtIndex = want;
        didWork = true;
    }
    return didWork;
}

static inline float macroShift (float v, double m)
{
    if (m > 0.0) v += (float) ((1.0 - v) * m);
    else         v += (float) (v * m);
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void EngineSlot::applyControls (AudioEffectX& fx, int engineIndex, const SlotParams& p, const MacroParams& m, bool force)
{
    const auto& info = EngineRegistry::get().info (engineIndex);
    for (size_t i = 0; i < info.controls.size(); ++i)
    {
        const auto& c = info.controls[i];
        float v = p.controls[i];
        switch (c.role)
        {
            case ParamRole::Size:     v = macroShift (v,  m.size);  break;
            case ParamRole::Decay:    v = macroShift (v,  m.decay); break;
            case ParamRole::DecayInv: v = macroShift (v, -m.decay); break;
            case ParamRole::Tone:     v = macroShift (v,  m.tone);  break;
            case ParamRole::ToneInv:  v = macroShift (v, -m.tone);  break;
            default: break;
        }
        const auto idx = (size_t) c.engineIndex;
        if (force || idx >= lastSent.size() || lastSent[idx] != v)
        {
            fx.setParameter (c.engineIndex, v);
            if (idx < lastSent.size()) lastSent[idx] = v;
        }
    }
}

void EngineSlot::process (const double* inL, const double* inR, double* outL, double* outR, int n,
                          const SlotParams& p, const MacroParams& macros)
{
    // Pick up a freshly built engine and start a crossfade.
    if (fading == nullptr)
    {
        if (auto* inc = incoming.exchange (nullptr))
        {
            fading = active.release();
            fadingIndex = activeIndex.load();
            active.reset (inc);
            activeIndex.store (incomingIndex.load());
            fadePos = 0;
            forceParams = true;
        }
    }

    // Pre-delay
    const double targetDelay = p.predelayMs * 0.001 * sampleRate;
    for (int i = 0; i < n; ++i)
    {
        // gentle protection: whatever upstream does, an engine never sees more than ~+6 dBFS
        double l = 2.0 * dsp::softClip (inL[i] * 0.5), r = 2.0 * dsp::softClip (inR[i] * 0.5);
        predelay.process (l, r, delaySmooth.next (targetDelay));
        preL[(size_t) i] = l; preR[(size_t) i] = r;
    }

    const auto& info = EngineRegistry::get().info (activeIndex.load());
    applyControls (*active, activeIndex.load(), p, macros, forceParams);
    forceParams = false;

    double* ins[2]  = { preL.data(), preR.data() };
    double* outs[2] = { outL, outR };
    active->processDoubleReplacing (ins, outs, n);

    bool crossfaded = false;
    if (fading != nullptr)
    {
        if (fadePos < fadeLen)
        {
            double* touts[2] = { tmpL.data(), tmpR.data() };
            fading->processDoubleReplacing (ins, touts, n);
            const double trimOld = dsp::dbToGain (EngineRegistry::get().info (fadingIndex).trimDb);
            const double trimNew = dsp::dbToGain (info.trimDb);
            for (int i = 0; i < n; ++i)
            {
                const double t = std::min (1.0, (double) (fadePos + i) / (double) fadeLen);
                const double gNew = std::sin (t * dsp::pi * 0.5), gOld = std::cos (t * dsp::pi * 0.5);
                outL[i] = outL[i] * gNew * trimNew + tmpL[(size_t) i] * gOld * trimOld;
                outR[i] = outR[i] * gNew * trimNew + tmpR[(size_t) i] * gOld * trimOld;
            }
            fadePos += n;
            crossfaded = true;
        }
        else
        {
            // Hand the old engine back for deletion; if the slot is still occupied, try next chunk.
            AudioEffectX* expected = nullptr;
            if (retired.compare_exchange_strong (expected, fading))
                fading = nullptr;
        }
    }

    if (! crossfaded)
    {
        const double trim = dsp::dbToGain (info.trimDb);
        for (int i = 0; i < n; ++i) { outL[i] *= trim; outR[i] *= trim; }
    }

    // Width, balance, level
    const double targetGain = dsp::dbToGain (p.levelDb); // mute is handled by the network
    double e = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double g = gainSmooth.next (targetGain);
        const double w = widthSmooth.next (p.width);
        const double pan = panSmooth.next (p.pan);
        const double mid = (outL[i] + outR[i]) * 0.5, side = (outL[i] - outR[i]) * 0.5 * w;
        double l = mid + side, r = mid - side;
        l *= g * std::min (1.0, 1.0 - pan);
        r *= g * std::min (1.0, 1.0 + pan);
        outL[i] = l; outR[i] = r;
        e += l * l + r * r;
    }
    energyState = energyState * 0.97 + 0.03 * std::sqrt (e / (2.0 * std::max (1, n)));
    energy.store ((float) energyState);
}

} // namespace airverbs
