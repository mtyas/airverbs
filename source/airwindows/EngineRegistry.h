#pragma once

#include <memory>
#include <string>
#include <vector>

class AudioEffectX;

namespace airverbs
{

/** Which global macro (if any) drives an engine parameter. */
enum class ParamRole { Size, Decay, DecayInv, Tone, ToneInv, Wet, Other };

struct EngineParamInfo
{
    std::string name;
    float defaultValue = 0.5f;
    ParamRole role = ParamRole::Other;
    bool hasRange = false;       // continuous with a numeric display range
    float displayMin = 0.0f, displayMax = 1.0f;
    std::vector<std::string> labels; // discrete step labels (empty = continuous)
    int engineIndex = 0;         // index into the Airwindows parameter list

    std::string formatValue (float normalised) const;
};

struct EngineInfo
{
    std::string name;
    int family = 0;
    float trimDb = 0.0f;
    std::string description;
    int numEngineParams = 0;               // raw Airwindows count, including wet
    int wetParamIndex = -1;                // forced to 100% wet inside Airverbs
    std::vector<EngineParamInfo> controls; // user-facing params (wet removed), max 6
    AudioEffectX* (*create)() = nullptr;
};

/** Static catalogue of all wrapped Airwindows reverbs. */
class EngineRegistry
{
public:
    static constexpr int maxControls = 6;

    static const EngineRegistry& get();

    int size() const noexcept                           { return (int) engines.size(); }
    const EngineInfo& info (int index) const            { return engines[(size_t) clampIndex (index)]; }
    int indexOf (const std::string& name) const;
    const std::vector<std::string>& familyNames() const { return families; }
    std::vector<std::string> engineNames() const;

    /** Creates a fresh engine at the given rate with Airwindows defaults and 100% wet. */
    std::unique_ptr<AudioEffectX> createEngine (int index, double sampleRate) const;

    int clampIndex (int i) const noexcept { return i < 0 ? 0 : (i >= (int) engines.size() ? (int) engines.size() - 1 : i); }

private:
    EngineRegistry();
    std::vector<EngineInfo> engines;
    std::vector<std::string> families;
};

} // namespace airverbs
