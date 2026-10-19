#include "EngineRegistry.h"
#include "audioeffectx.h"

#define AIRVERBS_ENGINE(n) AudioEffectX* aw_create_##n();
#include "engines/EngineList.inc"
#undef AIRVERBS_ENGINE

namespace airverbs
{

std::string EngineParamInfo::formatValue (float v) const
{
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    char buf[64];
    if (! labels.empty())
    {
        auto i = (size_t) (v * ((float) labels.size() - 0.001f));
        return labels[i < labels.size() ? i : labels.size() - 1];
    }
    if (hasRange)
    {
        const float x = displayMin + (displayMax - displayMin) * v;
        std::snprintf (buf, sizeof (buf), std::abs (displayMax - displayMin) >= 20.0f ? "%.1f" : "%.2f", x);
        return buf;
    }
    std::snprintf (buf, sizeof (buf), "%d%%", (int) std::lround (v * 100.0f));
    return buf;
}

namespace
{
    struct RawParam
    {
        const char* name; float def; ParamRole role; bool hasRange; float lo, hi;
        std::vector<std::string> labels;
    };
}

EngineRegistry::EngineRegistry()
{
#define AIRVERBS_FAMILY(f) families.push_back (f);
#include "FamilyList.inc"
#undef AIRVERBS_FAMILY

#define AIRVERBS_PARAM(n, d, r, hr, lo, hi, ...) RawParam { n, d, r, hr, lo, hi, std::vector<std::string> __VA_ARGS__ }
#define AIRVERBS_NO_PARAMS
#define AIRVERBS_ENGINE_INFO(n, fam, trim, desc, ...)                                  \
    {                                                                                  \
        EngineInfo e;                                                                  \
        e.name = #n; e.family = fam; e.trimDb = trim; e.description = desc;           \
        e.create = &aw_create_##n;                                                     \
        std::vector<RawParam> raw { __VA_ARGS__ };                                     \
        e.numEngineParams = (int) raw.size();                                          \
        for (int i = 0; i < (int) raw.size(); ++i)                                     \
        {                                                                              \
            auto& r = raw[(size_t) i];                                                 \
            if (r.role == ParamRole::Wet && e.wetParamIndex < 0) { e.wetParamIndex = i; continue; } \
            EngineParamInfo p;                                                         \
            p.name = r.name; p.defaultValue = r.def; p.role = r.role;                  \
            p.hasRange = r.hasRange; p.displayMin = r.lo; p.displayMax = r.hi;         \
            p.labels = r.labels; p.engineIndex = i;                                    \
            if ((int) e.controls.size() < maxControls) e.controls.push_back (p);       \
        }                                                                              \
        engines.push_back (std::move (e));                                             \
    }
#include "EngineInfo.inc"
#undef AIRVERBS_ENGINE_INFO
#undef AIRVERBS_PARAM
#undef AIRVERBS_NO_PARAMS
}

const EngineRegistry& EngineRegistry::get()
{
    static const EngineRegistry instance;
    return instance;
}

int EngineRegistry::indexOf (const std::string& name) const
{
    for (size_t i = 0; i < engines.size(); ++i)
        if (engines[i].name == name)
            return (int) i;
    return -1;
}

std::vector<std::string> EngineRegistry::engineNames() const
{
    std::vector<std::string> names;
    for (auto& e : engines)
        names.push_back (e.name);
    return names;
}

std::unique_ptr<AudioEffectX> EngineRegistry::createEngine (int index, double sampleRate) const
{
    const auto& e = info (index);
    std::unique_ptr<AudioEffectX> fx (e.create());
    fx->setSampleRate (sampleRate);
    for (auto& c : e.controls)
        fx->setParameter (c.engineIndex, c.defaultValue);
    if (e.wetParamIndex >= 0)
        fx->setParameter (e.wetParamIndex, 1.0f);
    return fx;
}

} // namespace airverbs
