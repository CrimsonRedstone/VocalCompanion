#pragma once

#include <JuceHeader.h>
#include "VisualTap.h"
#include "Comparison.h"
#include <complex>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <mutex>
#include <limits>

namespace vc
{

inline constexpr float kPi  = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;
inline constexpr float kDenorm = 1.0e-20f;

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float g) noexcept
{
    return 20.0f * std::log10 (std::max (g, 1.0e-8f));
}

inline float clamp (float x, float lo, float hi) noexcept
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float lerp (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

inline float fastTanh (float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float midiToHz (float midi) noexcept
{
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

inline float hzToMidi (float hz) noexcept
{
    return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0f) / 440.0f);
}

inline float onePole (float current, float target, float coeff) noexcept
{
    return current + coeff * (target - current);
}

inline float msToCoeff (float ms, double sr) noexcept
{
    if (ms <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp (-1.0f / (0.001f * ms * (float) sr));
}

//==============================================================================
struct ParamDesc
{
    juce::String id;
    juce::String label;
    float min;
    float max;
    float def;
    juce::String suffix;
    bool integer { false };
    juce::StringArray labels {};
};

struct ModuleColour
{
    juce::Colour fill;
    juce::Colour fillHi;
    juce::Colour accent;
};

enum class ModuleType
{
    Gain,
    DeEsser,
    FetComp,
    OptoComp,
    Limiter,
    DynamicEq,
    ParaEq,
    PitchFormant,
    AutoTune,
    Imager,      // WIDTH
    MsEq,
    AirBreath,
    Exciter,
    RingMod,
    Bitcrush,
    Chorus,
    Phaser,
    Tremolo,
    AutoPan,
    Delay,
    Reverb,
    External,
    Aeterna,
    BreathControl,
    VocalRider,
    PlosiveControl,
    WaveShaper,
    TubeOverdrive,
    RatDistortion
};

inline juce::String typeId (ModuleType t)
{
    switch (t)
    {
        case ModuleType::BreathControl: return "breathcontrol";
        case ModuleType::VocalRider: return "vocalrider";
        case ModuleType::PlosiveControl: return "plosivecontrol";
        case ModuleType::TubeOverdrive: return "tubeoverdrive";
        case ModuleType::RatDistortion: return "ratdistortion";
        case ModuleType::WaveShaper:     return "waveshaper";
        case ModuleType::Gain:          return "gain";
        case ModuleType::DeEsser:       return "deesser";
        case ModuleType::FetComp:       return "fetcomp";
        case ModuleType::OptoComp:      return "optocomp";
        case ModuleType::Limiter:       return "limiter";
        case ModuleType::DynamicEq:     return "dyneq";
        case ModuleType::ParaEq:        return "paraeq";
        case ModuleType::PitchFormant:  return "pitch";
        case ModuleType::AutoTune:      return "autotune";
        case ModuleType::Imager:        return "imager";
        case ModuleType::MsEq:          return "mseq";
        case ModuleType::AirBreath:     return "air";
        case ModuleType::Exciter:       return "exciter";
        case ModuleType::RingMod:       return "ringmod";
        case ModuleType::Bitcrush:      return "bitcrush";
        case ModuleType::Chorus:        return "chorus";
        case ModuleType::Phaser:        return "phaser";
        case ModuleType::Tremolo:       return "tremolo";
        case ModuleType::AutoPan:       return "autopan";
        case ModuleType::Delay:         return "delay";
        case ModuleType::Reverb:        return "reverb";
        case ModuleType::Aeterna:       return "aeterna";
        case ModuleType::External:      return "external";
    }
    return "gain";
}

inline juce::String typeName (ModuleType t)
{
    switch (t)
    {
        case ModuleType::BreathControl: return "BREATH CONTROL";
        case ModuleType::VocalRider: return "VOCAL RIDER";
        case ModuleType::PlosiveControl: return "PLOSIVE CONTROL";
        case ModuleType::TubeOverdrive: return "TUBE OVERDRIVE";
        case ModuleType::RatDistortion: return "RAT DISTORTION";
        case ModuleType::WaveShaper:     return "WAVE SHAPER";
        case ModuleType::Gain:          return "GAIN";
        case ModuleType::DeEsser:       return "DE-ESSER";
        case ModuleType::FetComp:       return "FET COMP";
        case ModuleType::OptoComp:      return "OPTO COMP";
        case ModuleType::Limiter:       return "LIMITER";
        case ModuleType::DynamicEq:     return "DYNAMIC EQ";
        case ModuleType::ParaEq:        return "PARAM EQ";
        case ModuleType::PitchFormant:  return "PITCH / FORMANT";
        case ModuleType::AutoTune:      return "AUTO TUNE";
        case ModuleType::Imager:        return "WIDTH";
        case ModuleType::MsEq:          return "M/S EQ";
        case ModuleType::AirBreath:     return "AIR / BREATH";
        case ModuleType::Exciter:       return "EXCITER";
        case ModuleType::RingMod:       return "RING MOD";
        case ModuleType::Bitcrush:      return "BITCRUSH";
        case ModuleType::Chorus:        return "CHORUS";
        case ModuleType::Phaser:        return "PHASER";
        case ModuleType::Tremolo:       return "TREMOLO";
        case ModuleType::AutoPan:       return "AUTO PAN";
        case ModuleType::Delay:         return "DELAY";
        case ModuleType::Reverb:       return "REVERB";
        case ModuleType::Aeterna:       return "AETERNA";
        case ModuleType::External:      return "EXTERNAL VST";
    }
    return "MODULE";
}

inline ModuleColour typeColour (ModuleType t)
{
    switch (t)
    {
        case ModuleType::BreathControl: return {juce::Colour(0xff244442),juce::Colour(0xff32615e),juce::Colour(0xff8ce5ce)};
        case ModuleType::VocalRider: return {juce::Colour(0xff45401e),juce::Colour(0xff655c2b),juce::Colour(0xffe9d485)};
        case ModuleType::PlosiveControl: return {juce::Colour(0xff383450),juce::Colour(0xff514b70),juce::Colour(0xffbbaaf0)};
        case ModuleType::TubeOverdrive: return {juce::Colour(0xff512719),juce::Colour(0xff71432b),juce::Colour(0xffffbd70)};
        case ModuleType::RatDistortion: return {juce::Colour(0xff37352d),juce::Colour(0xff555044),juce::Colour(0xffeddf9b)};
        case ModuleType::WaveShaper:     return { juce::Colour (0xff4a3020), juce::Colour (0xff684328), juce::Colour (0xffffb060) };
        case ModuleType::Gain:          return { juce::Colour (0xff1a4a5c), juce::Colour (0xff2a6a82), juce::Colour (0xff3ec8e0) };
        case ModuleType::DeEsser:       return { juce::Colour (0xff16345c), juce::Colour (0xff1e4a84), juce::Colour (0xff5aa0ff) };
        case ModuleType::FetComp:       return { juce::Colour (0xff6a2030), juce::Colour (0xff8a3044), juce::Colour (0xffff6a82) };
        case ModuleType::OptoComp:      return { juce::Colour (0xff5a3020), juce::Colour (0xff7a4430), juce::Colour (0xffffa070) };
        case ModuleType::Limiter:       return { juce::Colour (0xff4a1828), juce::Colour (0xff6a2438), juce::Colour (0xffff5070) };
        case ModuleType::DynamicEq:     return { juce::Colour (0xff1e3c32), juce::Colour (0xff2c5848), juce::Colour (0xff70e0b0) };
        case ModuleType::ParaEq:        return { juce::Colour (0xff16382c), juce::Colour (0xff245444), juce::Colour (0xff5ad4a0) };
        case ModuleType::PitchFormant:  return { juce::Colour (0xff32244c), juce::Colour (0xff483868), juce::Colour (0xffc0a0ff) };
        case ModuleType::AutoTune:      return { juce::Colour (0xff111827), juce::Colour (0xff00e5ff), juce::Colour (0xff7c4dff) };
        case ModuleType::Imager:        return { juce::Colour (0xff1a3848), juce::Colour (0xff285868), juce::Colour (0xff70d0e8) };
        case ModuleType::MsEq:          return { juce::Colour (0xff143040), juce::Colour (0xff204858), juce::Colour (0xff60c0d8) };
        case ModuleType::AirBreath:     return { juce::Colour (0xff1a3048), juce::Colour (0xff284868), juce::Colour (0xff90d8ff) };
        case ModuleType::Exciter:       return { juce::Colour (0xff4a2818), juce::Colour (0xff6a3c24), juce::Colour (0xffff9050) };
        case ModuleType::RingMod:       return { juce::Colour (0xff2a1838), juce::Colour (0xff402850), juce::Colour (0xffd080ff) };
        case ModuleType::Bitcrush:      return { juce::Colour (0xff301818), juce::Colour (0xff482424), juce::Colour (0xffff6868) };
        case ModuleType::Chorus:        return { juce::Colour (0xff3a4a20), juce::Colour (0xff526830), juce::Colour (0xffc8e060) };
        case ModuleType::Phaser:        return { juce::Colour (0xff2a3a18), juce::Colour (0xff3c5424), juce::Colour (0xffa8c050) };
        case ModuleType::Tremolo:       return { juce::Colour (0xff5a4a18), juce::Colour (0xff7a6828), juce::Colour (0xffe8d060) };
        case ModuleType::AutoPan:       return { juce::Colour (0xff1a4a4a), juce::Colour (0xff286868), juce::Colour (0xff50e0d0) };
        case ModuleType::Delay:         return { juce::Colour (0xff3a2060), juce::Colour (0xff523088), juce::Colour (0xffc090ff) };
        case ModuleType::Reverb:        return { juce::Colour (0xff242850), juce::Colour (0xff383c78), juce::Colour (0xff90a0ff) };
        case ModuleType::Aeterna:       return { juce::Colour (0xff30233d), juce::Colour (0xff503a60), juce::Colour (0xffe5c992) };
        case ModuleType::External:      return { juce::Colour (0xff2a2c32), juce::Colour (0xff3c4048), juce::Colour (0xffc8ccd4) };
    }
    return { juce::Colours::darkgrey, juce::Colours::grey, juce::Colours::white };
}

inline ModuleColour skinned (ModuleColour c, float hue)
{
    if (std::abs (hue) < 0.001f) return c;
    return { c.fill.withRotatedHue (hue), c.fillHi.withRotatedHue (hue), c.accent.withRotatedHue (hue) };
}

struct SkinDef
{
    const char* name;
    juce::uint32 bg, chrome, accent;
    float hue;
};

inline const SkinDef kSkins[] = {
    { "Default",    0xff07090c, 0xff16181f, 0xff3ec8e0, 0 },
    { "Leek",       0xff042422, 0xff0a4a44, 0xff39c5bb, 0 },
    { "Snow",       0xff0a1828, 0xff1a3a58, 0xffb8e8ff, 0 },
    { "Sakura",     0xff240814, 0xff4a1830, 0xffff8ab8, 0 },
    { "Baguette",   0xff280808, 0xff5a1018, 0xffff4040, 0 },
    { "Ice Cream",  0xff081028, 0xff182860, 0xff4a78ff, 0 },
    { "Cheers",     0xff281008, 0xff5a2818, 0xffff7040, 0 },
    { "Tako",       0xff240818, 0xff4a1838, 0xffff90c8, 0 },
    { "Roller",     0xff241808, 0xff4a3810, 0xffffd040, 0 },
    { "Banana",     0xff242008, 0xff4a4410, 0xffffe060, 0 },
    { "Gummy",      0xff082408, 0xff184818, 0xff70e050, 0 },
    { "Rocks",      0xff180824, 0xff301848, 0xffe0b0ff, 0 },
    { "Yuzu",       0xff140824, 0xff2c1848, 0xffa060ff, 0 },
    { "Violet",     0xff1c0828, 0xff381050, 0xffc040ff, 0 },
    { "See You",    0xff182408, 0xff304010, 0xffc8e040, 0 },
    { "Otama",      0xff1a1a1a, 0xff3a3a3a, 0xffd0d0d0, 0 },
    { "Solar",      0xff241808, 0xff4a3010, 0xfff0c060, 0 },
    { "Feather",    0xff081828, 0xff103848, 0xff66d4ff, 0 },
    { "Gimme",      0xff241408, 0xff4a2810, 0xffff9040, 0 },
    { "Sleepy",     0xff201808, 0xff3a3410, 0xffe8d048, 0 },
};
inline constexpr int kNumSkins = 20;

inline ModuleType typeFromId (const juce::String& id)
{
    if (id == "breathcontrol") return ModuleType::BreathControl;
    if (id == "vocalrider") return ModuleType::VocalRider;
    if (id == "plosivecontrol") return ModuleType::PlosiveControl;
    if(id=="tubeoverdrive")return ModuleType::TubeOverdrive;
    if(id=="ratdistortion")return ModuleType::RatDistortion;
    if (id == "waveshaper")    return ModuleType::WaveShaper;
    if (id == "deesser")      return ModuleType::DeEsser;
    if (id == "fetcomp")      return ModuleType::FetComp;
    if (id == "optocomp")     return ModuleType::OptoComp;
    if (id == "limiter")      return ModuleType::Limiter;
    if (id == "dyneq")        return ModuleType::DynamicEq;
    if (id == "paraeq")       return ModuleType::ParaEq;
    if (id == "pitch")        return ModuleType::PitchFormant;
    if (id == "autotune")     return ModuleType::AutoTune;
    if (id == "imager" || id == "width") return ModuleType::Imager;
    if (id == "mseq")         return ModuleType::MsEq;
    if (id == "air")          return ModuleType::AirBreath;
    if (id == "exciter")      return ModuleType::Exciter;
    if (id == "ringmod")      return ModuleType::RingMod;
    if (id == "bitcrush")     return ModuleType::Bitcrush;
    if (id == "chorus")       return ModuleType::Chorus;
    if (id == "phaser")       return ModuleType::Phaser;
    if (id == "tremolo")      return ModuleType::Tremolo;
    if (id == "autopan")      return ModuleType::AutoPan;
    if (id == "delay")        return ModuleType::Delay;
    if (id == "reverb")       return ModuleType::Reverb;
    if (id == "aeterna")      return ModuleType::Aeterna;
    if (id == "external")     return ModuleType::External;
    return ModuleType::Gain;
}

//==============================================================================
class Biquad
{
public:
    float magnitude (float frequency, double sr) const
    {
        const auto z = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * frequency / sr);
        return (float) std::abs (((double)b0 + (double)b1*z + (double)b2*z*z)
                              / (1.0 + (double)a1*z + (double)a2*z*z));
    }

    void setLowpass (float freq, float q, double sr)
    {
        const float w = kTwoPi * freq / (float) sr;
        const float a = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        const float b0n = (1.0f - c) * 0.5f;
        const float b1n = 1.0f - c;
        const float b2n = b0n;
        const float a0  = 1.0f + a;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a;
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setHighpass (float freq, float q, double sr)
    {
        const float w = kTwoPi * freq / (float) sr;
        const float a = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        const float b0n = (1.0f + c) * 0.5f;
        const float b1n = -(1.0f + c);
        const float b2n = b0n;
        const float a0  = 1.0f + a;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a;
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setBandpass (float freq, float q, double sr)
    {
        const float w = kTwoPi * freq / (float) sr;
        const float a = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        const float b0n = a;
        const float b1n = 0.0f;
        const float b2n = -a;
        const float a0  = 1.0f + a;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a;
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setPeak (float freq, float q, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * freq / (float) sr;
        const float a = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        const float b0n = 1.0f + a * A;
        const float b1n = -2.0f * c;
        const float b2n = 1.0f - a * A;
        const float a0  = 1.0f + a / A;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a / A;
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setHighShelf (float freq, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, clamp (gainDb, -24.0f, 24.0f) / 40.0f);
        const float w = kTwoPi * clamp (freq, 20.0f, (float) sr * 0.45f) / (float) sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float alpha = s * 0.5f * 1.41421356f; // S = 1
        const float twoSA = 2.0f * std::sqrt (A) * alpha;
        const float b0n =     A * ((A + 1.0f) + (A - 1.0f) * c + twoSA);
        const float b1n = -2.f * A * ((A - 1.0f) + (A + 1.0f) * c);
        const float b2n =     A * ((A + 1.0f) + (A - 1.0f) * c - twoSA);
        const float a0  =         ((A + 1.0f) - (A - 1.0f) * c + twoSA);
        const float a1n =  2.f *     ((A - 1.0f) - (A + 1.0f) * c);
        const float a2n =         ((A + 1.0f) - (A - 1.0f) * c - twoSA);
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setLowShelf (float freq, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, clamp (gainDb, -24.0f, 24.0f) / 40.0f);
        const float w = kTwoPi * clamp (freq, 20.0f, (float) sr * 0.45f) / (float) sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float alpha = s * 0.5f * 1.41421356f;
        const float twoSA = 2.0f * std::sqrt (A) * alpha;
        const float b0n =     A * ((A + 1.0f) - (A - 1.0f) * c + twoSA);
        const float b1n =  2.f * A * ((A - 1.0f) - (A + 1.0f) * c);
        const float b2n =     A * ((A + 1.0f) - (A - 1.0f) * c - twoSA);
        const float a0  =         ((A + 1.0f) + (A - 1.0f) * c + twoSA);
        const float a1n = -2.f *     ((A - 1.0f) + (A + 1.0f) * c);
        const float a2n =         ((A + 1.0f) + (A - 1.0f) * c - twoSA);
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    void setAllpass (float freq, double sr)
    {
        const float w = std::tan (kPi * clamp (freq, 20.0f, (float) sr * 0.45f) / (float) sr);
        const float a = (1.0f - w) / (1.0f + w);
        b0 = a; b1 = 1.0f; b2 = 0.0f;
        a1 = a; a2 = 0.0f;
    }

    /** Second-order allpass, pole at radius r and angle 2 pi f / sr. Unity mag. */
    void setAllpass2 (float freq, float radius, double sr)
    {
        const float r = juce::jlimit (0.1f, 0.98f, radius);
        const float th = kTwoPi * clamp (freq, 20.0f, (float) sr * 0.45f) / (float) sr;
        const float c = std::cos (th);
        a1 = -2.0f * r * c;
        a2 = r * r;
        b0 = a2;
        b1 = a1;
        b2 = 1.0f;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

private:
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
};

//==============================================================================
class EnvelopeFollower
{
public:
    void prepare (double sr, float attackMs, float releaseMs)
    {
        atk = msToCoeff (attackMs, sr);
        rel = msToCoeff (releaseMs, sr);
    }

    float process (float x) noexcept
    {
        const float a = std::abs (x);
        env = a > env ? env + atk * (a - env) : env + rel * (a - env);
        return env;
    }

    float current() const noexcept { return env; }
    void reset() noexcept { env = 0; }

private:
    float env = 0, atk = 0.1f, rel = 0.01f;
};

//==============================================================================
// Advanced parameters are appended: never move shipped core parameter IDs.
inline const std::vector<ParamDesc>& advancedDescriptors(ModuleType type)
{
    switch (type)
    {
        case ModuleType::Gain: {
            static const std::vector<ParamDesc> params {
                {"adv_harmonics", "Even harmonics", 0.0f, 2.0f, 1.0f, ""},
                {"adv_dc", "DC cutoff", 5.0f, 120.0f, 20.0f, " Hz"},
            }; return params;
        }
        case ModuleType::FetComp: {
            static const std::vector<ParamDesc> params {
                {"adv_sc_hp", "Detector HP", 20.0f, 600.0f, 20.0f, " Hz"},
                {"adv_colour", "FET colour", 0.0f, 1.0f, 1.0f, ""},
            }; return params;
        }
        case ModuleType::OptoComp: {
            static const std::vector<ParamDesc> params {
                {"adv_knee", "Knee", 0.0f, 18.0f, 6.0f, " dB"},
                {"adv_rms_window", "Detector window", 1.0f, 100.0f, 8.0f, " ms"},
            }; return params;
        }
        case ModuleType::DeEsser: {
            static const std::vector<ParamDesc> params {
                {"adv_harsh_freq", "Harsh frequency", 1500.0f, 6000.0f, 3200.0f, " Hz"},
                {"adv_sib_freq", "Sibilant frequency", 3500.0f, 12000.0f, 6200.0f, " Hz"},
                {"adv_attack", "Attack", 0.1f, 30.0f, 2.0f, " ms"},
            }; return params;
        }
        case ModuleType::Limiter: {
            static const std::vector<ParamDesc> params {
                {"adv_hold", "Peak hold", 0.0f, 100.0f, 0.0f, " ms"},
                {"adv_release_scale", "Release multiplier", 0.25f, 3.0f, 1.0f, "x"},
            }; return params;
        }
        case ModuleType::DynamicEq: {
            static const std::vector<ParamDesc> params {
                {"adv_dyn_threshold", "Dynamic threshold", -60.0f, 0.0f, -24.0f, " dB"},
                {"adv_dyn_attack", "Dynamic attack", 0.5f, 100.0f, 8.0f, " ms"},
                {"adv_dyn_release", "Dynamic release", 10.0f, 1000.0f, 90.0f, " ms"},
                {"adv_freq1", "Low frequency", 40.0f, 400.0f, 120.0f, " Hz"},
                {"adv_freq2", "Body frequency", 150.0f, 1000.0f, 400.0f, " Hz"},
                {"adv_freq3", "Honk frequency", 500.0f, 3000.0f, 1200.0f, " Hz"},
                {"adv_freq4", "Presence frequency", 1500.0f, 8000.0f, 3500.0f, " Hz"},
            }; return params;
        }
        case ModuleType::ParaEq: {
            static const std::vector<ParamDesc> params {
                {"adv_q1", "Band 1 Q", 0.2f, 12.0f, 1.1f, ""},
                {"adv_q2", "Band 2 Q", 0.2f, 12.0f, 1.1f, ""},
                {"adv_q3", "Band 3 Q", 0.2f, 12.0f, 1.1f, ""},
                {"adv_hp_q", "High-pass Q", 0.5f, 2.0f, 0.7f, ""},
                {"adv_lp_q", "Low-pass Q", 0.5f, 2.0f, 0.7f, ""},
            }; return params;
        }
        case ModuleType::MsEq: {
            static const std::vector<ParamDesc> params {
                {"adv_mid_freq", "Mid bell frequency", 200.0f, 6000.0f, 1200.0f, " Hz"},
                {"adv_side_freq", "Side bell frequency", 200.0f, 6000.0f, 1800.0f, " Hz"},
                {"adv_bell_q", "Bell Q", 0.2f, 8.0f, 0.9f, ""},
            }; return params;
        }
        case ModuleType::AirBreath: {
            static const std::vector<ParamDesc> params {
                {"adv_shelf_freq", "Air frequency", 3000.0f, 12000.0f, 8000.0f, " Hz"},
                {"adv_noise_level", "Breath texture", 0.0f, 2.0f, 1.0f, ""},
            }; return params;
        }
        case ModuleType::Exciter: {
            static const std::vector<ParamDesc> params {
                {"adv_high_split", "Harmonic crossover", 800.0f, 8000.0f, 2800.0f, " Hz"},
                {"adv_low_split", "Warmth crossover", 80.0f, 1500.0f, 500.0f, " Hz"},
            }; return params;
        }
        case ModuleType::RingMod: {
            static const std::vector<ParamDesc> params {
                {"adv_shape", "Carrier shape", 0.0f, 1.0f, 0.0f, ""},
                {"adv_bias", "Carrier bias", 0.0f, 1.0f, 0.0f, ""},
            }; return params;
        }
        case ModuleType::Bitcrush: {
            static const std::vector<ParamDesc> params {
                {"adv_quant_curve", "Quantizer curve", 0.25f, 4.0f, 1.0f, ""},
                {"adv_stereo", "Stereo preserve", 0.0f, 1.0f, 0.0f, ""},
            }; return params;
        }
        case ModuleType::Chorus: {
            static const std::vector<ParamDesc> params {
                {"adv_stereo_offset", "Stereo offset", 0.0f, 24.0f, 3.0f, " samples"},
                {"adv_shape", "Triangle blend", 0.0f, 1.0f, 0.0f, ""},
            }; return params;
        }
        case ModuleType::Phaser: {
            static const std::vector<ParamDesc> params {
                {"adv_span", "Sweep span", 0.25f, 3.0f, 1.0f, "x"},
                {"adv_phase", "LFO phase", 0.0f, 360.0f, 0.0f, " deg"},
            }; return params;
        }
        case ModuleType::Tremolo: {
            static const std::vector<ParamDesc> params {
                {"adv_phase", "LFO phase", 0.0f, 360.0f, 0.0f, " deg"},
                {"adv_curve", "Envelope curve", 0.25f, 4.0f, 1.0f, ""},
            }; return params;
        }
        case ModuleType::AutoPan: {
            static const std::vector<ParamDesc> params {
                {"adv_shape", "Triangle blend", 0.0f, 1.0f, 0.0f, ""},
                {"adv_centre", "Pan centre", -1.0f, 1.0f, 0.0f, ""},
            }; return params;
        }
        case ModuleType::Delay: {
            static const std::vector<ParamDesc> params {
                {"adv_duck", "Wet ducking", 0.0f, 1.0f, 0.0f, ""},
                {"adv_duck_release", "Duck release", 20.0f, 1000.0f, 180.0f, " ms"},
            }; return params;
        }
        case ModuleType::Reverb: {
            static const std::vector<ParamDesc> params {
                {"adv_predelay", "Predelay", 0.0f, 200.0f, 0.0f, " ms"},
                {"adv_duck_release", "Duck release", 20.0f, 1200.0f, 220.0f, " ms"},
            }; return params;
        }
        case ModuleType::BreathControl: {
            static const std::vector<ParamDesc> params {
                {"adv_attack", "Reduction attack", 1.0f, 100.0f, 12.0f, " ms"},
                {"adv_min_breath", "Minimum breath", 0.0f, 150.0f, 25.0f, " ms"},
            }; return params;
        }
        case ModuleType::VocalRider: {
            static const std::vector<ParamDesc> params {
                {"adv_rms_window", "RMS window", 10.0f, 400.0f, 80.0f, " ms"},
                {"adv_relax", "Silence return", 50.0f, 2000.0f, 500.0f, " ms"},
            }; return params;
        }
        case ModuleType::PlosiveControl: {
            static const std::vector<ParamDesc> params {
                {"adv_attack", "Burst attack", 0.1f, 20.0f, 1.0f, " ms"},
                {"adv_hold", "Burst hold", 0.0f, 150.0f, 25.0f, " ms"},
                {"adv_lf_ratio", "Low-frequency ratio", 0.1f, 0.9f, 0.35f, ""},
            }; return params;
        }
        case ModuleType::TubeOverdrive: {
            static const std::vector<ParamDesc> params {
                {"adv_input_hp", "Input low cut", 20.0f, 500.0f, 60.0f, " Hz"},
                {"adv_bias", "Tube bias", -0.4f, 0.4f, 0.0f, ""},
            }; return params;
        }
        case ModuleType::RatDistortion: {
            static const std::vector<ParamDesc> params {
                {"adv_input_hp", "Input low cut", 20.0f, 500.0f, 80.0f, " Hz"},
                {"adv_clip", "Diode threshold", 0.2f, 1.2f, 0.65f, ""},
            }; return params;
        }
        case ModuleType::WaveShaper: {
            static const std::vector<ParamDesc> params {
                {"adv_bias", "Input bias", -0.5f, 0.5f, 0.0f, ""},
                {"adv_dc", "DC rejection", 5.0f, 120.0f, 35.18f, " Hz"},
            }; return params;
        }
        case ModuleType::PitchFormant: {
            static const std::vector<ParamDesc> params {
                {"adv_transpose", "Transpose", -12.0f, 12.0f, 0.0f, " st"},
                {"adv_amount", "Correction amount", 0.0f, 1.0f, 1.0f, ""},
            }; return params;
        }
        case ModuleType::AutoTune: {
            static const std::vector<ParamDesc> params {
                {"adv_amount", "Correction amount", 0.0f, 1.0f, 1.0f, ""},
                {"adv_hysteresis", "Note hysteresis", 0.0f, 50.0f, 12.0f, " cents"},
            }; return params;
        }
        case ModuleType::Imager: {
            static const std::vector<ParamDesc> params {
                {"adv_side_gain", "Original side", 0.0f, 2.0f, 1.0f, "x"},
                {"adv_release", "Transient recovery", 5.0f, 250.0f, 40.0f, " ms"},
            }; return params;
        }
        case ModuleType::Aeterna: {
            static const std::vector<ParamDesc> params {
                {"adv_choir_width", "Choir width", 0.0f, 1.0f, 1.0f, ""},
                {"adv_glass_feedback", "Glass resonance", 0.5f, 0.97f, 0.91f, ""},
                {"adv_shimmer_feed", "Shimmer feedback", 0.0f, 0.55f, 0.38f, ""},
                {"adv_choir_formant", "Choir formant", -6.0f, 6.0f, 0.0f, " st"},
            }; return params;
        }
        default: { static const std::vector<ParamDesc> empty; return empty; }
    }
}

/** Lightweight DSP module contract. Stock modules implement this; the host
    processor walks the chain in processBlock. Parameters live as atomics so
    the editor can write from the message thread without a lock. */
class VcModule
{
public:
    virtual ~VcModule() = default;

    virtual ModuleType getType() const = 0;
    virtual juce::String getDisplayName() const { return typeName (getType()); }

    virtual void prepare (double sampleRate, int samplesPerBlock, int numChannels) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;
    virtual void handleMidi (const juce::MidiMessage&) {}
    virtual void clearMidi() {}
    virtual void processWithMidi(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer&, int) { process(buffer); }

    void setBypassed (bool b) noexcept { bypassed.store (b, std::memory_order_relaxed); }
    bool isBypassed() const noexcept { return bypassed.load (std::memory_order_relaxed); }
    void setSoloed (bool b) noexcept { soloed.store (b, std::memory_order_relaxed); }
    bool isSoloed() const noexcept { return soloed.load (std::memory_order_relaxed); }

    virtual const std::vector<ParamDesc>& getCoreParamDescs() const = 0;
    virtual float getCoreParam (int index) const = 0;
    virtual void setCoreParam (int index, float value) = 0;

    // One public parameter path handles host automation, preset recall, A/B and
    // both editors. DSP reads advanced atomics directly, without a UI dependency.
    int coreParamCount() const { return (int)getCoreParamDescs().size(); }
    const std::vector<ParamDesc>& getParamDescs() const
    {
        std::call_once(descriptorInit, [this] {
            allDescriptors = getCoreParamDescs();
            const auto& extra = advancedDescriptors(getType());
            allDescriptors.insert(allDescriptors.end(), extra.begin(), extra.end());
        });
        return allDescriptors;
    }
    float getParam(int index) const
    {
        if (index < 0) return 0;
        const int core = coreParamCount();
        return index < core ? getCoreParam(index) : advanced(index - core);
    }
    void setParam(int index, float value)
    {
        if (index < 0 || !std::isfinite(value)) return;
        const int core = coreParamCount();
        if (index < core) { setCoreParam(index, value); return; }
        const auto& extra = advancedDescriptors(getType());
        const int slot = index - core;
        if (slot >= (int)extra.size()) return;
        const auto& d = extra[(size_t)slot];
        advancedValues[(size_t)slot].store(clamp(value, d.min, d.max));
    }
    float advanced(int slot) const
    {
        const auto& extra = advancedDescriptors(getType());
        if (slot < 0 || slot >= (int)extra.size()) return 0;
        const float value = advancedValues[(size_t)slot].load();
        return std::isfinite(value) ? value : extra[(size_t)slot].def;
    }

    virtual float getVisualEqGain (int i) const { return getParam (i); }
    virtual float getVisualPhase() const { return 0; }
    virtual float getMeter (int /*index*/ = 0) const { return 0.0f; }
    virtual int   getNumMeters() const { return 0; }
    virtual double tailLengthSeconds() const { return 0.0; }
    virtual int   latencySamples() const { return 0; }
    virtual int   copyPitchTrace (float* /*inMidi*/, float* /*outMidi*/, int /*maxN*/) const { return 0; }

    virtual juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("MODULE");
        t.setProperty ("type", typeId (getType()), nullptr);
        t.setProperty ("bypass", isBypassed(), nullptr);
        t.setProperty ("solo", isSoloed(), nullptr);
        t.setProperty ("id", instanceId.toString(), nullptr);
        const auto& d = getParamDescs();
        for (int i = 0; i < (int) d.size(); ++i)
            t.setProperty (d[(size_t) i].id, getParam (i), nullptr);
        return t;
    }

    virtual void fromValueTree (const juce::ValueTree& t)
    {
        setBypassed ((bool) t.getProperty ("bypass", false));
        setSoloed ((bool) t.getProperty ("solo", false));
        if (t.hasProperty ("id"))
            instanceId = juce::Uuid (t.getProperty ("id").toString());
        for (auto& value : advancedValues) value.store(NAN);
        const auto& d = getParamDescs();
        for (int i = 0; i < (int) d.size(); ++i)
            if (t.hasProperty (d[(size_t) i].id))
                setParam (i, (float) t.getProperty (d[(size_t) i].id));
    }

    // Slots are initialized after construction/loading, before the first edit.
    // They hold virtual state too (including hosted plug-in state and version tags).
    void initialiseSnapshots()
    {
        snapshots[0]=toValueTree();snapshots[1]=snapshots[0].createCopy();
        comparison.mode=0;
    }
    void selectSnapshot(int slot)
    {
        slot=juce::jlimit(0,1,slot);const int active=comparison.mode.load();
        if(slot==active)return;
        if(!snapshots[0].isValid())initialiseSnapshots();
        snapshots[(size_t)active]=toValueTree();
        fromValueTree(snapshots[(size_t)slot]);
        comparison.mode=slot;
    }
    juce::ValueTree stateWithSnapshots() const
    {
        auto current=toValueTree();juce::ValueTree ab("CARD_AB");
        const int active=comparison.mode.load();ab.setProperty("selected",active,nullptr);
        ab.setProperty("match",comparison.matchEnabled.load(),nullptr);
        for(int i=0;i<2;++i)
        {
            juce::ValueTree slot(i==0?"A":"B");
            slot.appendChild((i==active||!snapshots[(size_t)i].isValid()?current:snapshots[(size_t)i]).createCopy(),nullptr);
            ab.appendChild(slot,nullptr);
        }
        current.appendChild(ab,nullptr);return current;
    }
    void restoreSnapshots(const juce::ValueTree& state)
    {
        initialiseSnapshots();
        const auto ab=state.getChildWithName("CARD_AB");
        if(!ab.isValid())return;
        for(int i=0;i<2;++i)
        {
            const auto saved=ab.getChildWithName(i==0?"A":"B").getChild(0);
            if(saved.hasType("MODULE")&&saved.getProperty("type")==state.getProperty("type"))snapshots[(size_t)i]=saved.createCopy();
        }
        comparison.mode=juce::jlimit(0,1,(int)ab.getProperty("selected",0));
        comparison.matchEnabled=(bool)ab.getProperty("match",true);
    }

    Comparison comparison;
    std::atomic<float> cpuPercent {0};
    std::atomic<float> latencyMs {0};
    bool wasProcessing = false; // audio-thread only
    std::shared_ptr<VisualTap> visual = std::make_shared<VisualTap>();
    juce::Uuid instanceId;
    double currentBpm { 120.0 };

private:
    mutable std::once_flag descriptorInit;
    mutable std::vector<ParamDesc> allDescriptors;
    std::array<std::atomic<float>,8> advancedValues {{NAN,NAN,NAN,NAN,NAN,NAN,NAN,NAN}};
    std::array<juce::ValueTree,2> snapshots;
protected:
    std::atomic<bool> bypassed { false };
    std::atomic<bool> soloed { false };
};

} // namespace vc
