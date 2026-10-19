// Dev tool: instantiates every wrapped Airwindows engine and prints its parameter
// metadata as JSON (names, defaults, discrete step labels). Consumed by tools/gen_engine_info.py.
#include "audioeffectx.h"
#include <cstdio>
#include <string>
#include <vector>
#define AIRVERBS_ENGINE(n) AudioEffectX* aw_create_##n();
#include "EngineList.inc"
#undef AIRVERBS_ENGINE

static std::string esc (const char* s) { std::string r; for (; *s; ++s) { if (*s == '"' || *s == '\\') r += '\\'; r += *s; } return r; }

static void dump (const char* name, AudioEffectX* e, bool& first)
{
    std::printf ("%s{\"name\":\"%s\",\"params\":[", first ? "" : ",\n", name);
    first = false;
    for (int i = 0; i < e->getNumParameters(); ++i)
    {
        char t[128] = {0};
        e->getParameterName (i, t);
        const float def = e->getParameter (i);
        // probe for discrete labels
        std::vector<std::string> labels;
        bool discrete = false;
        for (int k = 0; k <= 400; ++k)
        {
            const float v = (float) k / 400.0f;
            e->setParameter (i, v);
            char d[128] = {0}, f[128] = {0};
            e->getParameterDisplay (i, d);
            float2string (v, f, kVstMaxParamStrLen);
            if (std::string (d) != f) discrete = true;
            if (labels.empty() || labels.back() != d) labels.push_back (d);
        }
        e->setParameter (i, def);
        std::printf ("%s{\"name\":\"%s\",\"def\":%.4f,\"labels\":[", i ? "," : "", esc (t).c_str(), def);
        if (discrete)
            for (size_t k = 0; k < labels.size(); ++k)
                std::printf ("%s\"%s\"", k ? "," : "", esc (labels[k].c_str()).c_str());
        std::printf ("]}");
    }
    std::printf ("]}");
}

int main()
{
    bool first = true;
    std::printf ("[\n");
#define AIRVERBS_ENGINE(n) { AudioEffectX* e = aw_create_##n(); dump (#n, e, first); delete e; }
#include "EngineList.inc"
    std::printf ("\n]\n");
}
