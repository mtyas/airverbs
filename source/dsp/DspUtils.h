#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace airverbs::dsp
{

constexpr double pi = 3.14159265358979323846;

inline double dbToGain (double db) { return db <= -47.9 ? 0.0 : std::pow (10.0, db / 20.0); }
inline double clamp01 (double x)   { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

/** Airwindows-flavoured soft clip: sine-shaped knee, hard ceiling at +-1. */
inline double softClip (double x)
{
    if (x > 1.5707963) return 1.0;
    if (x < -1.5707963) return -1.0;
    return std::sin (x);
}

/** One-pole smoother for control values. */
struct Smoother
{
    double current = 0.0, coeff = 0.0;
    void reset (double sr, double timeMs, double value)
    {
        coeff = std::exp (-1.0 / (std::max (1.0, timeMs) * 0.001 * sr));
        current = value;
    }
    double next (double target) { current = target + coeff * (current - target); return current; }
    void snap (double v) { current = v; }
};

/** Topology-preserving-transform state variable filter (Zavalishin), one channel. */
struct Svf
{
    double g = 0.0, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0;
    double ic1 = 0.0, ic2 = 0.0;

    void setup (double freq, double sr, double q = 0.70710678)
    {
        freq = std::clamp (freq, 10.0, sr * 0.49);
        g = std::tan (pi * freq / sr);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.0; }

    inline void tick (double v0, double& low, double& high)
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        low = v2;
        high = v0 - k * v1 - v2;
    }
    inline double lowpass (double x)  { double l, h; tick (x, l, h); return l; }
    inline double highpass (double x) { double l, h; tick (x, l, h); return h; }
};

/** Linkwitz-Riley 4th order split (two cascaded Butterworth SVFs per branch). */
struct LR4Split
{
    Svf lp1, lp2, hp1, hp2;
    void setup (double f, double sr) { lp1.setup (f, sr); lp2.setup (f, sr); hp1.setup (f, sr); hp2.setup (f, sr); }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline void process (double x, double& low, double& high)
    {
        low  = lp2.lowpass (lp1.lowpass (x));
        high = hp2.highpass (hp1.highpass (x));
    }
};

/** Stereo delay line with smoothed fractional read position. */
class StereoDelay
{
public:
    void prepare (int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 4) size <<= 1;
        mask = size - 1;
        bufL.assign ((size_t) size, 0.0);
        bufR.assign ((size_t) size, 0.0);
        write = 0;
    }
    void clear() { std::fill (bufL.begin(), bufL.end(), 0.0); std::fill (bufR.begin(), bufR.end(), 0.0); }

    inline void process (double& l, double& r, double delaySamples)
    {
        bufL[(size_t) write] = l;
        bufR[(size_t) write] = r;
        if (delaySamples >= 1.0)
        {
            const double readPos = (double) write - std::min (delaySamples, (double) size - 3.0);
            const double fl = std::floor (readPos);
            const double frac = readPos - fl;
            const int i0 = (int) fl & mask, i1 = (i0 + 1) & mask;
            l = bufL[(size_t) i0] + frac * (bufL[(size_t) i1] - bufL[(size_t) i0]);
            r = bufR[(size_t) i0] + frac * (bufR[(size_t) i1] - bufR[(size_t) i0]);
        }
        write = (write + 1) & mask;
    }

private:
    std::vector<double> bufL, bufR;
    int size = 0, mask = 0, write = 0;
};

/** Peak envelope follower with separate attack/release. */
struct EnvelopeFollower
{
    double env = 0.0, att = 0.0, rel = 0.0;
    void setup (double sr, double attackMs, double releaseMs)
    {
        att = std::exp (-1.0 / (attackMs * 0.001 * sr));
        rel = std::exp (-1.0 / (releaseMs * 0.001 * sr));
    }
    inline double process (double x)
    {
        x = std::abs (x);
        env = x > env ? x + att * (env - x) : x + rel * (env - x);
        return env;
    }
};

} // namespace airverbs::dsp
