#include "Constellation.h"

namespace airverbs
{

using namespace dsp;

void Constellation::prepare (double sr, int /*maxBlock*/)
{
    sampleRate = sr;
    {
        const std::lock_guard<std::mutex> lock (loaderMutex);
        for (auto& s : slots)
            s.prepare (sr, chunk);
    }

    for (auto* a : { &slotInL, &slotInR, &slotOutL, &slotOutR, &prevOutL, &prevOutR })
        for (auto& b : *a) b.fill (0.0);
    fbL.fill (0.0); fbR.fill (0.0);

    for (auto& x : xoverL) x.reset();
    for (auto& x : xoverR) x.reset();
    lastCrossover = -1.0;

    fastEnv.setup (sr, 0.5, 20.0);
    slowEnv.setup (sr, 40.0, 400.0);
    duckEnv.setup (sr, 2.0, 300.0);
    transientSmooth.reset (sr, 5.0, 0.0);

    for (auto* f : { &fbHpL, &fbHpR }) { f->setup (60.0, sr); f->reset(); }
    for (auto* f : { &fbLpL, &fbLpR }) { f->setup (9000.0, sr); f->reset(); }
    fbGainState = loopRms = freezeTarget = crossfeedRms = networkInputEnv = 0.0;
    wasFrozen = false;
    inputGate.reset (sr, 50.0, 1.0);

    for (int i = 0; i < 2; ++i) { lowCutL[i].reset(); lowCutR[i].reset(); highCutL[i].reset(); highCutR[i].reset(); }
    lastLowCut = lastHighCut = -1.0;
    mixSmooth.reset (sr, 30.0, 0.35);
    outSmooth.reset (sr, 30.0, 1.0);
    inSmooth.reset (sr, 30.0, 1.0);
    widthSmooth.reset (sr, 30.0, 1.0);
    morphSmooth.reset (sr, 60.0, 0.5);
    duckSmooth.reset (sr, 30.0, 0.0);
    for (auto& w : weightSmooth) w.reset (sr, 25.0, 0.5);
    for (auto& m : muteSmooth) m.reset (sr, 15.0, 1.0);
    fbRingL.fill (0.0); fbRingR.fill (0.0); fbWrite = 0;
    peakL = peakR = 0.0;
}

bool Constellation::serviceLoaders()
{
    const std::lock_guard<std::mutex> lock (loaderMutex);
    bool any = false;
    for (auto& s : slots) any |= s.service();
    return any;
}

void Constellation::computeWeights (double x, double y, std::array<double, 4>& w) const
{
    // Corners match the GUI: A top-left, B top-right, C bottom-left, D bottom-right.
    w[0] = (1.0 - x) * (1.0 - y);
    w[1] = x * (1.0 - y);
    w[2] = (1.0 - x) * y;
    w[3] = x * y;
    for (auto& v : w) v = std::sqrt (std::max (0.0, v)); // equal power
}

void Constellation::updateOrbit (const NetworkParams& p, int n)
{
    const double dt = (double) n / sampleRate;
    orbitPhase += p.orbitRate * dt;
    orbitPhase -= std::floor (orbitPhase);
    const double ph = orbitPhase * 2.0 * pi;
    double ox = 0.0, oy = 0.0;
    switch (p.orbitShape)
    {
        case OrbitShape::Circle:    ox = std::cos (ph); oy = std::sin (ph); break;
        case OrbitShape::Figure8:   ox = std::sin (ph); oy = std::sin (2.0 * ph); break;
        case OrbitShape::Lissajous: ox = std::sin (3.0 * ph); oy = std::sin (2.0 * ph + 0.5); break;
        case OrbitShape::Pendulum:  ox = std::sin (ph); oy = 0.0; break;
        case OrbitShape::Drift:
        {
            driftClock += p.orbitRate * dt;
            if (driftClock >= 1.0)
            {
                driftClock -= std::floor (driftClock);
                std::uniform_real_distribution<double> d (-1.0, 1.0);
                driftTX = d (rng); driftTY = d (rng);
            }
            const double c = 1.0 - std::exp (-dt * std::max (0.05, p.orbitRate) * 3.0);
            driftX += (driftTX - driftX) * c;
            driftY += (driftTY - driftY) * c;
            ox = driftX; oy = driftY;
            break;
        }
        default: break;
    }
    curX = clamp01 (p.vecX + p.orbitDepth * 0.5 * ox);
    curY = clamp01 (p.vecY + p.orbitDepth * 0.5 * oy);
}

void Constellation::process (float* const* ch, int numCh, int numSamples, const NetworkParams& p)
{
    if (numCh <= 0) return;
    float* L = ch[0];
    float* R = numCh > 1 ? ch[1] : ch[0];

    std::array<double, chunk> inL {}, inR {}, outL {}, outR {};
    for (int start = 0; start < numSamples; start += chunk)
    {
        const int n = std::min (chunk, numSamples - start);
        for (int i = 0; i < n; ++i) { inL[(size_t) i] = L[start + i]; inR[(size_t) i] = R[start + i]; }
        processChunk (inL.data(), inR.data(), outL.data(), outR.data(), n, p);
        if (numCh > 1)
            for (int i = 0; i < n; ++i) { L[start + i] = (float) outL[(size_t) i]; R[start + i] = (float) outR[(size_t) i]; }
        else
            for (int i = 0; i < n; ++i) L[start + i] = (float) (0.5 * (outL[(size_t) i] + outR[(size_t) i]));
    }
}

void Constellation::processChunk (const double* inL, const double* inR, double* outL, double* outR, int n, const NetworkParams& p)
{
    const auto topo = p.topology;
    const bool splitTopology = topo == Topology::Prism || topo == Topology::Dimension || topo == Topology::Pulsar;
    const bool seriesTopology = topo == Topology::Chain || topo == Topology::Twin || topo == Topology::Bloom;

    updateOrbit (p, n);
    std::array<double, 4> targetW;
    computeWeights (curX, curY, targetW);
    if (splitTopology)
        for (auto& w : targetW) w = std::min (1.5, w * 2.0); // split bands must sum to unity at the centre

    const double morph = morphSmooth.next (p.morph);
    for (int i = 1; i < n; ++i) morphSmooth.next (p.morph);

    // ---- feedback loop gain
    if (p.freeze && ! wasFrozen)
    {
        freezeTarget = std::clamp (loopRms, 0.03, 0.2);
        fbGainState = std::max (fbGainState, 0.9);
    }
    wasFrozen = p.freeze;
    if (p.freeze)
    {
        const double ratio = freezeTarget / std::max (loopRms, 1.0e-5);
        fbGainState = std::clamp (fbGainState * std::pow (ratio, ratio < 1.0 ? 0.15 : 0.03), 0.0, 2.0);
    }
    else
    {
        double target = p.feedback * 0.95;
        if (loopRms > 0.2) target *= std::pow (0.2 / loopRms, 2.0); // keep runaway regeneration civil
        fbGainState += (target - fbGainState) * (target < fbGainState ? 0.3 : 0.05);
    }

    // ---- build network input
    const double inGainT = dbToGain (p.inputDb);
    double inEnergy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double gIn = inSmooth.next (inGainT) * inputGate.next (p.freeze ? 0.0 : 1.0);
        dryL[(size_t) i] = inL[i];
        dryR[(size_t) i] = inR[i];
        inEnergy += inL[i] * inL[i] + inR[i] * inR[i];

        const int readIdx = (fbWrite - chunk + i) & (fbRingSize - 1);
        double fl = fbHpL.highpass (fbLpL.lowpass (fbRingL[(size_t) readIdx]));
        double fr = fbHpR.highpass (fbLpR.lowpass (fbRingR[(size_t) readIdx]));
        fl = softClip (fl * fbGainState);
        fr = softClip (fr * fbGainState);

        netL[(size_t) i] = inL[i] * gIn + fl;
        netR[(size_t) i] = inR[i] * gIn + fr;
    }

    auto runSlot = [&] (int k)
    {
        slots[(size_t) k].process (slotInL[(size_t) k].data(), slotInR[(size_t) k].data(),
                                   slotOutL[(size_t) k].data(), slotOutR[(size_t) k].data(), n, p.slots[(size_t) k], p.macros);
    };
    // In series topologies a muted star becomes a bypass: its input continues downstream.
    auto forward = [&] (int k, int dest, double addDry, const double* srcL, const double* srcR)
    {
        const bool byp = p.slots[(size_t) k].mute;
        for (int i = 0; i < n; ++i)
        {
            const double oL = byp ? slotInL[(size_t) k][(size_t) i] : slotOutL[(size_t) k][(size_t) i];
            const double oR = byp ? slotInR[(size_t) k][(size_t) i] : slotOutR[(size_t) k][(size_t) i];
            slotInL[(size_t) dest][(size_t) i] = oL + addDry * srcL[i];
            slotInR[(size_t) dest][(size_t) i] = oR + addDry * srcR[i];
        }
    };
    auto setInput = [&] (int k, const double* l, const double* r, double g = 1.0)
    {
        for (int i = 0; i < n; ++i) { slotInL[(size_t) k][(size_t) i] = l[i] * g; slotInR[(size_t) k][(size_t) i] = r[i] * g; }
    };

    const double* XL = netL.data();
    const double* XR = netR.data();

    switch (topo)
    {
        case Topology::Nebula:
        {
            // Each star hears the others (one chunk later). The crossfeed is level-governed so the
            // star-to-star loop can bloom but never run away.
            double xfSq = 0.0;
            for (int k = 0; k < 4; ++k)
                for (int i = 0; i < n; ++i)
                    xfSq += prevOutL[(size_t) k][(size_t) i] * prevOutL[(size_t) k][(size_t) i]
                          + prevOutR[(size_t) k][(size_t) i] * prevOutR[(size_t) k][(size_t) i];
            crossfeedRms = crossfeedRms * 0.9 + 0.1 * std::sqrt (xfSq / (8.0 * n));
            // The crossfeed may only ever be as loud as what is being played into the network
            // (with a slow release), so the star-to-star bloom always dies away once input stops.
            double inSq = 0.0;
            for (int i = 0; i < n; ++i) inSq += XL[i] * XL[i] + XR[i] * XR[i];
            const double inRms = std::sqrt (inSq / (2.0 * n));
            const double rel = std::exp (-(double) n / (sampleRate * (0.4 + 2.0 * morph)));
            networkInputEnv = inRms > networkInputEnv ? inRms : networkInputEnv * rel;
            double govern = crossfeedRms > 0.12 ? 0.12 / crossfeedRms : 1.0;
            govern *= std::min (1.0, networkInputEnv / (crossfeedRms + 1.0e-9));
            const double c = morph * 0.45 / 3.0 * govern;
            for (int k = 0; k < 4; ++k)
            {
                for (int i = 0; i < n; ++i)
                {
                    double sl = 0.0, sr = 0.0;
                    for (int j = 0; j < 4; ++j)
                        if (j != k) { sl += prevOutL[(size_t) j][(size_t) i]; sr += prevOutR[(size_t) j][(size_t) i]; }
                    slotInL[(size_t) k][(size_t) i] = XL[i] + softClip (sl * c);
                    slotInR[(size_t) k][(size_t) i] = XR[i] + softClip (sr * c);
                }
                runSlot (k);
            }
            break;
        }
        case Topology::Chain:
        {
            setInput (0, XL, XR);
            for (int k = 0; k < 4; ++k)
            {
                runSlot (k);
                if (k < 3)
                {
                    std::array<double, chunk> srcL, srcR;
                    std::copy_n (slotInL[(size_t) k].begin(), n, srcL.begin());
                    std::copy_n (slotInR[(size_t) k].begin(), n, srcR.begin());
                    forward (k, k + 1, morph, srcL.data(), srcR.data());
                }
            }
            break;
        }
        case Topology::Twin:
        {
            std::array<double, chunk> aL, aR, cL, cR;
            for (int i = 0; i < n; ++i)
            {
                aL[(size_t) i] = XL[i];
                aR[(size_t) i] = XR[i] + (XL[i] - XR[i]) * morph;
                cL[(size_t) i] = XL[i] + (XR[i] - XL[i]) * morph;
                cR[(size_t) i] = XR[i];
            }
            setInput (0, aL.data(), aR.data());
            setInput (2, cL.data(), cR.data());
            runSlot (0); runSlot (2);
            forward (0, 1, 0.0, aL.data(), aR.data());
            forward (2, 3, 0.0, cL.data(), cR.data());
            runSlot (1); runSlot (3);
            break;
        }
        case Topology::Bloom:
        {
            setInput (0, XL, XR);
            runSlot (0);
            for (int k = 1; k < 4; ++k) { forward (0, k, morph, XL, XR); runSlot (k); }
            break;
        }
        case Topology::Prism:
        {
            const double c1 = 120.0 * std::pow (2.0, p.morph * 3.3);
            if (std::abs (c1 - lastCrossover) > 0.5)
            {
                const double fs[3] = { c1, c1 * 4.0, std::min (c1 * 16.0, sampleRate * 0.45) };
                for (int b = 0; b < 3; ++b) { xoverL[(size_t) b].setup (fs[b], sampleRate); xoverR[(size_t) b].setup (fs[b], sampleRate); }
                lastCrossover = c1;
            }
            for (int i = 0; i < n; ++i)
            {
                double lo, hi, b0, b1, b2, b3;
                xoverL[1].process (XL[i], lo, hi); xoverL[0].process (lo, b0, b1); xoverL[2].process (hi, b2, b3);
                slotInL[0][(size_t) i] = b0; slotInL[1][(size_t) i] = b1; slotInL[2][(size_t) i] = b2; slotInL[3][(size_t) i] = b3;
                xoverR[1].process (XR[i], lo, hi); xoverR[0].process (lo, b0, b1); xoverR[2].process (hi, b2, b3);
                slotInR[0][(size_t) i] = b0; slotInR[1][(size_t) i] = b1; slotInR[2][(size_t) i] = b2; slotInR[3][(size_t) i] = b3;
            }
            for (int k = 0; k < 4; ++k) runSlot (k);
            break;
        }
        case Topology::Dimension:
        {
            for (int i = 0; i < n; ++i)
            {
                const double m = (XL[i] + XR[i]) * 0.5, s = (XL[i] - XR[i]) * 0.5;
                for (int k = 0; k < 2; ++k) { slotInL[(size_t) k][(size_t) i] = m; slotInR[(size_t) k][(size_t) i] = m; }
                for (int k = 2; k < 4; ++k) { slotInL[(size_t) k][(size_t) i] = s; slotInR[(size_t) k][(size_t) i] = -s; }
            }
            for (int k = 0; k < 4; ++k) runSlot (k);
            const double sideGain = 1.0 + morph * 1.5;
            for (int k = 2; k < 4; ++k)
                for (int i = 0; i < n; ++i)
                {
                    const double s = (slotOutL[(size_t) k][(size_t) i] - slotOutR[(size_t) k][(size_t) i]) * 0.5 * sideGain;
                    slotOutL[(size_t) k][(size_t) i] = s; slotOutR[(size_t) k][(size_t) i] = -s;
                }
            break;
        }
        case Topology::Pulsar:
        {
            const double sens = 0.5 + p.morph * 6.0;
            double tAcc = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double m = (XL[i] + XR[i]) * 0.5;
                const double f = fastEnv.process (m), sl = slowEnv.process (m);
                const double t = clamp01 ((f / (sl + 1.0e-6) - 1.0) * sens * 0.5);
                const double ts = transientSmooth.next (t);
                tAcc += ts;
                for (int k = 0; k < 2; ++k) { slotInL[(size_t) k][(size_t) i] = XL[i] * ts; slotInR[(size_t) k][(size_t) i] = XR[i] * ts; }
                for (int k = 2; k < 4; ++k) { slotInL[(size_t) k][(size_t) i] = XL[i] * (1.0 - ts); slotInR[(size_t) k][(size_t) i] = XR[i] * (1.0 - ts); }
            }
            telemetry.transient.store ((float) (tAcc / std::max (1, n)));
            for (int k = 0; k < 4; ++k) runSlot (k);
            break;
        }
        default: break;
    }

    // ---- sum the stars through the vector weights
    std::array<double, 4> wNow {};
    std::array<double, 4> slotEnergy {};
    for (int i = 0; i < n; ++i)
    {
        double l = 0.0, r = 0.0;
        for (int k = 0; k < 4; ++k)
        {
            const double w = weightSmooth[(size_t) k].next (targetW[(size_t) k])
                           * muteSmooth[(size_t) k].next (p.slots[(size_t) k].mute ? 0.0 : 1.0);
            wNow[(size_t) k] = w;
            const double sl = slotOutL[(size_t) k][(size_t) i] * w, sr = slotOutR[(size_t) k][(size_t) i] * w;
            slotEnergy[(size_t) k] += sl * sl + sr * sr;
            l += sl; r += sr;
        }
        wetL[(size_t) i] = l; wetR[(size_t) i] = r;
    }
    for (int k = 0; k < 4; ++k)
    {
        std::copy_n (slotOutL[(size_t) k].begin(), n, prevOutL[(size_t) k].begin());
        std::copy_n (slotOutR[(size_t) k].begin(), n, prevOutR[(size_t) k].begin());
        std::fill (prevOutL[(size_t) k].begin() + n, prevOutL[(size_t) k].end(), 0.0);
        std::fill (prevOutR[(size_t) k].begin() + n, prevOutR[(size_t) k].end(), 0.0);
        prevEnergyL[(size_t) k] = prevEnergyL[(size_t) k] * 0.9 + 0.1 * std::sqrt (slotEnergy[(size_t) k] / (2.0 * n));
        telemetry.weight[(size_t) k].store ((float) wNow[(size_t) k]);
        telemetry.energy[(size_t) k].store ((float) prevEnergyL[(size_t) k]);
    }

    // ---- feedback ring + loop level
    double wetEnergy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        fbRingL[(size_t) fbWrite] = wetL[(size_t) i];
        fbRingR[(size_t) fbWrite] = wetR[(size_t) i];
        fbWrite = (fbWrite + 1) & (fbRingSize - 1);
        wetEnergy += wetL[(size_t) i] * wetL[(size_t) i] + wetR[(size_t) i] * wetR[(size_t) i];
    }
    const double chunkRms = std::sqrt (wetEnergy / (2.0 * n));
    loopRms = loopRms * 0.95 + chunkRms * 0.05;

    // ---- output stage: width, cuts, ducking, mix
    if (std::abs (p.lowCut - lastLowCut) > 0.01)
    {
        for (int s = 0; s < 2; ++s) { lowCutL[s].setup (p.lowCut, sampleRate); lowCutR[s].setup (p.lowCut, sampleRate); }
        lastLowCut = p.lowCut;
    }
    if (std::abs (p.highCut - lastHighCut) > 0.01)
    {
        for (int s = 0; s < 2; ++s) { highCutL[s].setup (p.highCut, sampleRate); highCutR[s].setup (p.highCut, sampleRate); }
        lastHighCut = p.highCut;
    }
    const bool doLow = p.lowCut > 20.5, doHigh = p.highCut < 19900.0;
    const double outT = dbToGain (p.outputDb);
    for (int i = 0; i < n; ++i)
    {
        double l = wetL[(size_t) i], r = wetR[(size_t) i];
        const double w = widthSmooth.next (p.width);
        const double m = (l + r) * 0.5, s = (l - r) * 0.5 * w;
        l = m + s; r = m - s;
        if (doLow)  { for (int st = 0; st < 2; ++st) { l = lowCutL[st].highpass (l);  r = lowCutR[st].highpass (r); } }
        if (doHigh) { for (int st = 0; st < 2; ++st) { l = highCutL[st].lowpass (l); r = highCutR[st].lowpass (r); } }

        const double env = duckEnv.process ((dryL[(size_t) i] + dryR[(size_t) i]) * 0.5);
        const double duckG = 1.0 - duckSmooth.next (p.duck) * clamp01 (env * 4.0) * 0.95;

        const double mx = mixSmooth.next (p.mix);
        const double dryG = std::cos (mx * pi * 0.5), wetG = std::sin (mx * pi * 0.5);
        const double og = outSmooth.next (outT);
        outL[i] = (dryL[(size_t) i] * dryG + l * wetG * duckG) * og;
        outR[i] = (dryR[(size_t) i] * dryG + r * wetG * duckG) * og;
        peakL = std::max (std::abs (outL[i]), peakL * 0.9995);
        peakR = std::max (std::abs (outR[i]), peakR * 0.9995);
    }

    telemetry.posX.store ((float) curX);
    telemetry.posY.store ((float) curY);
    telemetry.inputLevel.store ((float) std::sqrt (inEnergy / (2.0 * n)));
    telemetry.wetLevel.store ((float) loopRms);
    telemetry.loopGain.store ((float) fbGainState);
    telemetry.outPeakL.store ((float) peakL);
    telemetry.outPeakR.store ((float) peakR);
    if (topo != Topology::Pulsar) telemetry.transient.store (0.0f);
}

} // namespace airverbs
