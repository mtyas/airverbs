// Minimal stand-in for the Steinberg VST2 AudioEffectX base class.
// Lets the original Airwindows LinuxVST sources compile unmodified so Airverbs
// can host them as internal engines. Only the API surface those sources use is provided.
#pragma once
#ifndef __audioeffect__
#define __audioeffect__
#endif

#ifndef _USE_MATH_DEFINES
 #define _USE_MATH_DEFINES // Airwindows uses M_PI; MSVC only defines it on request
#endif
#include <cmath>
#include <math.h>
#include <cstdint>
#include <stdint.h>
#include <cstdlib>
#include <stdlib.h>
#include <cstdio>
#include <stdio.h>
#include <cstring>
#include <string.h>
#include <set>
#include <string>
#include <algorithm>

typedef int32_t VstInt32;
typedef intptr_t VstIntPtr;
typedef VstIntPtr (*audioMasterCallback) (void*, VstInt32, VstInt32, VstIntPtr, void*, float);

enum VstPlugCategory { kPlugCategUnknown = 0, kPlugCategEffect, kPlugCategRoomFx };

enum
{
    kVstMaxProgNameLen   = 24,
    kVstMaxParamStrLen   = 8,
    kVstMaxVendorStrLen  = 64,
    kVstMaxProductStrLen = 64,
    kVstMaxEffectNameLen = 32
};

inline char* vst_strncpy (char* dst, const char* src, size_t maxLen)
{
    std::strncpy (dst, src, maxLen);
    dst[maxLen] = 0;
    return dst;
}

inline void float2string (float value, char* text, VstInt32 maxLen)
{
    std::snprintf (text, (size_t) maxLen + 1, "%.3f", value);
}

inline void int2string (VstInt32 value, char* text, VstInt32 maxLen)
{
    std::snprintf (text, (size_t) maxLen + 1, "%d", (int) value);
}

class AudioEffect
{
public:
    AudioEffect (audioMasterCallback, VstInt32, VstInt32 numParams) : numParameters (numParams) {}
    virtual ~AudioEffect() = default;

    void setSampleRate (double sr) { sampleRate = sr; }
    double getSampleRate() const   { return sampleRate; }
    VstInt32 getNumParameters() const { return numParameters; }

    void setNumInputs (VstInt32) {}
    void setNumOutputs (VstInt32) {}
    void setUniqueID (VstInt32) {}
    void canProcessReplacing (bool = true) {}
    void canDoubleReplacing (bool = true) {}
    void programsAreChunks (bool = true) {}

    virtual void processReplacing (float**, float**, VstInt32) = 0;
    virtual void processDoubleReplacing (double**, double**, VstInt32) = 0;
    virtual float getParameter (VstInt32) = 0;
    virtual void setParameter (VstInt32, float) = 0;
    virtual void getParameterName (VstInt32, char*) {}
    virtual void getParameterDisplay (VstInt32, char*) {}
    virtual void getParameterLabel (VstInt32, char*) {}

protected:
    double sampleRate = 44100.0;
    VstInt32 numParameters = 0;
};

class AudioEffectX : public AudioEffect
{
public:
    using AudioEffect::AudioEffect;
};
