// Offline checks for the Airverbs DSP. No JUCE, no host.
//   AirverbsTests survey [trims.json]   per-engine loudness / decay / CPU survey (+ writes trims)
//   AirverbsTests network               topology, feedback, freeze and hot-swap stress tests
#include "../source/dsp/Constellation.h"
#include "audioeffectx.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

using namespace airverbs;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Noise
{
    std::minstd_rand rng { 42 };
    double b0 = 0, b1 = 0, b2 = 0;
    double pink()
    {
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        const double w = d (rng);
        b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        return (b0 + b1 + b2 + w * 0.1848) * 0.11;
    }
};

static int survey (const char* trimsPath)
{
    const double sr = 48000.0;
    const auto& reg = EngineRegistry::get();
    std::vector<std::pair<std::string, double>> trims;
    std::printf ("%-16s %8s %8s %8s %8s %8s\n", "engine", "rms dB", "peak dB", "T30 s", "cpu %", "trim dB");
    for (int e = 0; e < reg.size(); ++e)
    {
        auto fx = reg.createEngine (e, sr);
        Noise noise;
        const int burst = (int) (0.4 * sr), total = (int) (8.0 * sr), chunk = 32;
        std::vector<double> inL (chunk), inR (chunk), outL (chunk), outR (chunk);
        double sumSq = 0.0, peak = 0.0;
        long count = 0;
        std::vector<double> env; // 10 ms RMS blocks
        double blockSq = 0.0; int blockN = 0;
        bool finite = true;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int pos = 0; pos < total; pos += chunk)
        {
            for (int i = 0; i < chunk; ++i)
            {
                const bool on = pos + i < burst;
                inL[(size_t) i] = on ? noise.pink() * 0.5 : 0.0;
                inR[(size_t) i] = on ? noise.pink() * 0.5 : 0.0;
            }
            double* ins[2] = { inL.data(), inR.data() };
            double* outs[2] = { outL.data(), outR.data() };
            fx->processDoubleReplacing (ins, outs, chunk);
            for (int i = 0; i < chunk; ++i)
            {
                const double l = outL[(size_t) i], r = outR[(size_t) i];
                if (! std::isfinite (l) || ! std::isfinite (r)) finite = false;
                peak = std::max ({ peak, std::abs (l), std::abs (r) });
                if (pos + i < burst + (int) (1.0 * sr)) { sumSq += l * l + r * r; count += 2; }
                blockSq += l * l + r * r; blockN += 2;
                if (blockN >= (int) (0.01 * sr) * 2) { env.push_back (std::sqrt (blockSq / blockN)); blockSq = 0; blockN = 0; }
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        const double rms = std::sqrt (sumSq / std::max (1L, count));
        const double rmsDb = 20.0 * std::log10 (rms + 1e-12);
        // T30: time from the envelope peak until it has fallen 30 dB
        size_t pk = 0;
        for (size_t i = 0; i < env.size(); ++i) if (env[i] > env[pk]) pk = i;
        double t30 = -1.0;
        for (size_t i = pk; i < env.size(); ++i)
            if (env[i] < env[pk] * 0.0316) { t30 = (double) (i - pk) * 0.01; break; }
        // Steady-state power gain with continuous pink noise: what a star sees when fed by another star.
        double ssIn = 0.0, ssOut = 0.0;
        {
            auto fx2 = reg.createEngine (e, sr);
            Noise n2;
            const int len = (int) (8.0 * sr);
            for (int pos = 0; pos < len; pos += chunk)
            {
                for (int i = 0; i < chunk; ++i) { inL[(size_t) i] = n2.pink() * 0.5; inR[(size_t) i] = n2.pink() * 0.5; }
                double* ins[2] = { inL.data(), inR.data() };
                double* outs[2] = { outL.data(), outR.data() };
                fx2->processDoubleReplacing (ins, outs, chunk);
                if (pos > (int) (4.0 * sr))
                    for (int i = 0; i < chunk; ++i)
                    {
                        ssIn += inL[(size_t) i] * inL[(size_t) i] + inR[(size_t) i] * inR[(size_t) i];
                        ssOut += outL[(size_t) i] * outL[(size_t) i] + outR[(size_t) i] * outR[(size_t) i];
                    }
            }
        }
        const double gainDb = 10.0 * std::log10 ((ssOut + 1e-20) / (ssIn + 1e-20));
        // Blend: half "how loud a burst sounds", half "steady-state power gain", capped so a fed star
        // never exceeds +3 dB of power gain (keeps chains and loops civil).
        const double burstTrim = -20.0 - rmsDb, steadyTrim = -gainDb - 3.0;
        const double trim = std::clamp (std::min (0.5 * (burstTrim + steadyTrim), steadyTrim + 6.0), -15.0, 15.0);
        trims.push_back ({ reg.info (e).name, trim });
        std::printf ("%-16s %8.1f %8.1f %8.2f %8.2f %8.1f  (steady gain %+.1f dB)%s\n", reg.info (e).name.c_str(), rmsDb, 20 * std::log10 (peak + 1e-12), t30,
                     secs / 8.0 * 100.0, trim, gainDb, finite ? "" : "  NON-FINITE!");
        CHECK (finite, "%s produced NaN/Inf", reg.info (e).name.c_str());
        CHECK (rmsDb > -80.0, "%s is silent at default settings", reg.info (e).name.c_str());
    }
    if (trimsPath != nullptr)
    {
        std::ofstream f (trimsPath);
        f << "{\n";
        for (size_t i = 0; i < trims.size(); ++i)
            f << "  \"" << trims[i].first << "\": " << trims[i].second << (i + 1 < trims.size() ? ",\n" : "\n");
        f << "}\n";
        std::printf ("wrote %s\n", trimsPath);
    }
    return failures;
}

static void setDefaults (NetworkParams& p, const std::array<const char*, 4>& engines)
{
    for (int s = 0; s < 4; ++s)
    {
        auto& sp = p.slots[(size_t) s];
        sp.engine = EngineRegistry::get().indexOf (engines[(size_t) s]);
        const auto& info = EngineRegistry::get().info (sp.engine);
        for (size_t k = 0; k < info.controls.size(); ++k) sp.controls[k] = info.controls[k].defaultValue;
    }
    p.mix = 1.0;
}

struct RunStats { double rms = 0, peak = 0; bool finite = true; };

static RunStats run (Constellation& net, const NetworkParams& p, double seconds, bool withInput, Noise& noise, double sr, int block = 256)
{
    RunStats st;
    std::vector<float> L ((size_t) block), R ((size_t) block);
    float* ch[2] = { L.data(), R.data() };
    const long total = (long) (seconds * sr);
    double sq = 0; long n = 0;
    for (long pos = 0; pos < total; pos += block)
    {
        const int len = (int) std::min<long> (block, total - pos);
        for (int i = 0; i < len; ++i)
        {
            const bool on = withInput && ((pos + i) % (long) sr) < (long) (0.3 * sr);
            L[(size_t) i] = on ? (float) noise.pink() * 0.5f : 0.0f;
            R[(size_t) i] = on ? (float) noise.pink() * 0.5f : 0.0f;
        }
        net.process (ch, 2, len, p);
        for (int i = 0; i < len; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) st.finite = false;
            st.peak = std::max ({ st.peak, (double) std::abs (L[(size_t) i]), (double) std::abs (R[(size_t) i]) });
            sq += (double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]; n += 2;
        }
    }
    st.rms = std::sqrt (sq / std::max (1L, n));
    return st;
}

static int networkTests()
{
    const double sr = 48000.0;
    Noise noise;
    auto net = std::make_unique<Constellation>();
    NetworkParams p;
    setDefaults (p, { "kStation", "kPlateB", "kCathedral5", "Galactic3" });
    for (int s = 0; s < 4; ++s) net->slot (s).requestEngine (p.slots[(size_t) s].engine);
    net->prepare (sr, 256);

    std::printf ("topologies (8 s each, default stars):\n");
    for (int t = 0; t < (int) Topology::count; ++t)
    {
        p.topology = (Topology) t;
        for (double morph : { 0.0, 0.5, 1.0 })
        {
            p.morph = morph;
            const auto st = run (*net, p, 8.0, true, noise, sr);
            std::printf ("  %-10s morph %.1f  rms %6.1f dB  peak %6.1f dB\n", topologyName (t), morph, 20 * std::log10 (st.rms + 1e-12), 20 * std::log10 (st.peak + 1e-12));
            CHECK (st.finite, "%s produced NaN/Inf", topologyName (t));
            CHECK (st.peak < 8.0, "%s peak too high (%.2f)", topologyName (t), st.peak);
            CHECK (st.rms > 1.0e-4, "%s is silent", topologyName (t));
        }
    }

    std::printf ("regen at 100%% for 30 s:\n");
    p.topology = Topology::Nebula; p.morph = 0.6; p.feedback = 1.0;
    {
        const auto st = run (*net, p, 30.0, true, noise, sr);
        std::printf ("  rms %.1f dB peak %.1f dB\n", 20 * std::log10 (st.rms + 1e-12), 20 * std::log10 (st.peak + 1e-12));
        CHECK (st.finite && st.peak < 8.0, "regen loop unstable");
    }

    std::printf ("freeze holds energy:\n");
    p.feedback = 0.0; p.freeze = false;
    run (*net, p, 2.0, true, noise, sr);
    p.freeze = true;
    const auto a = run (*net, p, 5.0, false, noise, sr);
    const auto b = run (*net, p, 20.0, false, noise, sr);
    std::printf ("  first 5 s %.1f dB, next 20 s %.1f dB, peak %.1f dB\n", 20 * std::log10 (a.rms + 1e-12), 20 * std::log10 (b.rms + 1e-12), 20 * std::log10 (b.peak + 1e-12));
    CHECK (b.finite && b.peak < 8.0, "freeze unstable");
    CHECK (b.rms > 1.0e-3, "freeze died away");
    p.freeze = false;
    const auto c = run (*net, p, 20.0, false, noise, sr);
    std::printf ("  after release (20 s) %.1f dB\n", 20 * std::log10 (c.rms + 1e-12));
    CHECK (c.rms < b.rms, "tail did not decay after unfreeze");

    std::printf ("hot-swap stress (engines change while audio runs):\n");
    {
        std::atomic<bool> stop { false };
        std::thread loader ([&] { while (! stop) { net->serviceLoaders(); std::this_thread::sleep_for (std::chrono::milliseconds (1)); } });
        std::minstd_rand rng (7);
        bool finite = true;
        for (int i = 0; i < 120; ++i)
        {
            const int s = (int) (rng() % 4), e = (int) (rng() % (unsigned) EngineRegistry::get().size());
            p.slots[(size_t) s].engine = e;
            net->slot (s).requestEngine (e);
            p.topology = (Topology) (rng() % (unsigned) Topology::count);
            finite &= run (*net, p, 0.05, true, noise, sr, 64 + (int) (rng() % 900)).finite;
        }
        stop = true;
        loader.join();
        CHECK (finite, "hot swap produced NaN/Inf");
        std::printf ("  120 swaps done\n");
    }

    std::printf ("sample-rate changes:\n");
    for (double rate : { 44100.0, 96000.0, 192000.0 })
    {
        net->prepare (rate, 512);
        const auto st = run (*net, p, 3.0, true, noise, rate, 512);
        std::printf ("  %.0f Hz: rms %.1f dB\n", rate, 20 * std::log10 (st.rms + 1e-12));
        CHECK (st.finite, "non-finite at %.0f", rate);
    }

    std::printf ("CPU: four heaviest-ish stars, 10 s at 48 kHz:\n");
    {
        setDefaults (p, { "VerbSevens", "kCathedral5", "MV2", "kRockstar2" });
        for (int s = 0; s < 4; ++s) net->slot (s).requestEngine (p.slots[(size_t) s].engine);
        net->prepare (sr, 256);
        p.topology = Topology::Nebula; p.feedback = 0.3;
        auto t0 = std::chrono::high_resolution_clock::now();
        run (*net, p, 10.0, true, noise, sr);
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        std::printf ("  %.2f%% of one core\n", secs / 10.0 * 100.0);
    }
    return failures;
}

int main (int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "network";
    int f = 0;
    if (mode == "survey") f = survey (argc > 2 ? argv[2] : nullptr);
    else f = networkTests();
    std::printf (f == 0 ? "\nALL PASSED\n" : "\n%d FAILURE(S)\n", f);
    return f == 0 ? 0 : 1;
}
