#pragma once
#include "Common.h"

namespace vc
{

/** Envelope-ducked pink-ish air/breath above 8 kHz + a real high-shelf on the
    dry signal so an analyser actually sees the air band. */
class AirBreathModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::AirBreath; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        hp.setHighpass (7000.0f, 0.7f, sr);
        env = 0;
        noise = 0;
        reset();
    }
    void reset() override
    {
        hp.reset();
        for (int c = 0; c < 2; ++c) { hs[c].reset(); pk[c].reset(); }
        env = 0;
        noise = 0;
    }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float amt = amount.load();
        const float dk = duck.load();
        const float shelfDb = amt * 14.0f;
        const float peakDb  = amt * 6.5f;
        for (int c = 0; c < 2; ++c)
        {
            hs[c].setHighShelf (8000.0f, shelfDb, sampleRate);
            pk[c].setPeak (11000.0f, 0.75f, peakDb, sampleRate);
        }
        hp.setHighpass (7000.0f, 0.7f, sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);
        const float atk = msToCoeff (4.0f, sampleRate);
        const float rel = msToCoeff (80.0f, sampleRate);
        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < chs; ++c)
                peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));
            env += (peak > env ? atk : rel) * (peak - env);
            noise = noise * 0.97f + (rng.nextFloat() * 2.0f - 1.0f) * 0.03f;
            const float breath = hp.process (noise) * amt * 0.55f;
            const float g = 1.0f - dk * clamp (env * 3.2f, 0.0f, 0.85f);
            const float add = breath * (0.2f + 0.8f * clamp (env * 6.0f, 0.0f, 1.0f)) * (0.35f + 0.65f * g);
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                x = pk[c].process (hs[c].process (x));
                buffer.setSample (c, i, x + add * (c ? 0.88f : 1.0f));
            }
        }
    }
    float getParam (int i) const override { return i == 0 ? amount.load() : duck.load(); }
    void setParam (int i, float v) override
    {
        if (i == 0) amount.store (clamp (v, 0.0f, 1.0f));
        else duck.store (clamp (v, 0.0f, 1.0f));
    }
private:
    std::atomic<float> amount { 0.35f }, duck { 0.5f };
    Biquad hp, hs[2], pk[2];
    juce::Random rng;
    float noise { 0 }, env { 0 };
    double sampleRate { 44100 };
    inline static const std::vector<ParamDesc> descs {
        { "amt", "Air", 0, 1, 0.35f, "" },
        { "duck", "Duck", 0, 1, 0.5f, "" }
    };
};

/** Upper-mid harmonics + low-mid tube warmth. */
class ExciterModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Exciter; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        for (int c = 0; c < 2; ++c)
        {
            hip[c].setHighpass (2800.0f, 0.7f, sr);
            lop[c].setLowpass (500.0f, 0.7f, sr);
        }
    }
    void reset() override { for (int c = 0; c < 2; ++c) { hip[c].reset(); lop[c].reset(); } }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float hi = harm.load();
        const float lo = warmth.load();
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                float h = hip[c].process (x);
                h = fastTanh (h * (1.0f + hi * 6.0f));
                float w = lop[c].process (x);
                w = fastTanh (w * (1.0f + lo * 3.0f));
                buffer.setSample (c, i, x + h * hi * 0.35f + w * lo * 0.22f);
            }
    }
    float getParam (int i) const override { return i == 0 ? harm.load() : warmth.load(); }
    void setParam (int i, float v) override
    {
        if (i == 0) harm.store (clamp (v, 0.0f, 1.0f));
        else warmth.store (clamp (v, 0.0f, 1.0f));
    }
private:
    std::atomic<float> harm { 0.35f }, warmth { 0.15f };
    Biquad hip[2], lop[2];
    double sampleRate { 44100 };
    inline static const std::vector<ParamDesc> descs {
        { "harm", "Harm", 0, 1, 0.35f, "" },
        { "warm", "Warm", 0, 1, 0.15f, "" }
    };
};

/** 3-node parametric EQ + HPF/LPF. Graphic dots drive gain (and freq). */
class ParaEqModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::ParaEq; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    void prepare (double sr, int, int) override { sampleRate = sr; reset(); }
    void reset() override
    {
        for (int c = 0; c < 2; ++c)
        {
            hp[c].reset(); lp[c].reset();
            for (int b = 0; b < 3; ++b) peak[c][b].reset();
        }
    }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float hpf = hpHz.load(), lpf = lpHz.load();
        const float f[3] = { freq[0].load(), freq[1].load(), freq[2].load() };
        const float g[3] = { gain[0].load(), gain[1].load(), gain[2].load() };
        for (int c = 0; c < 2; ++c)
        {
            hp[c].setHighpass (hpf, 0.7f, sampleRate);
            lp[c].setLowpass (lpf, 0.7f, sampleRate);
            for (int b = 0; b < 3; ++b)
                peak[c][b].setPeak (f[b], 1.1f, g[b], sampleRate);
        }
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                x = hp[c].process (x);
                x = lp[c].process (x);
                for (int b = 0; b < 3; ++b)
                    if (std::abs (g[b]) > 0.02f)
                        x = peak[c][b].process (x);
                buffer.setSample (c, i, x);
            }
    }
    float getParam (int i) const override
    {
        if (i < 3) return gain[i].load();
        if (i < 6) return freq[i - 3].load();
        return i == 6 ? hpHz.load() : lpHz.load();
    }
    void setParam (int i, float v) override
    {
        if (i < 3) gain[i].store (clamp (v, -18.0f, 18.0f));
        else if (i < 6) freq[i - 3].store (clamp (v, 40.0f, 16000.0f));
        else if (i == 6) hpHz.store (clamp (v, 20.0f, 400.0f));
        else lpHz.store (clamp (v, 4000.0f, 20000.0f));
    }
private:
    std::atomic<float> gain[3] { 0, 0, 0 };
    std::atomic<float> freq[3] { 120.0f, 1000.0f, 4500.0f };
    std::atomic<float> hpHz { 40.0f }, lpHz { 18000.0f };
    Biquad peak[2][3], hp[2], lp[2];
    double sampleRate { 44100 };
    inline static const std::vector<ParamDesc> descs {
        { "g1", "G1", -18, 18, 0, " dB" },
        { "g2", "G2", -18, 18, 0, " dB" },
        { "g3", "G3", -18, 18, 0, " dB" },
        { "f1", "F1", 40, 16000, 120, " Hz" },
        { "f2", "F2", 40, 16000, 1000, " Hz" },
        { "f3", "F3", 40, 16000, 4500, " Hz" },
        { "hp", "HPF", 20, 400, 40, " Hz" },
        { "lp", "LPF", 4000, 20000, 18000, " Hz" }
    };
};

class RingModModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::RingMod; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    void prepare (double sr, int, int) override { sampleRate = sr; phase = 0; }
    void reset() override { phase = 0; }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float hz = freq.load();
        const float mix = mixP.load();
        const float inc = kTwoPi * hz / (float) sampleRate;
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            const float car = std::sin (phase);
            phase += inc;
            if (phase > kTwoPi) phase -= kTwoPi;
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                buffer.setSample (c, i, lerp (x, x * car, mix));
            }
        }
    }
    float getParam (int i) const override { return i == 0 ? freq.load() : mixP.load(); }
    void setParam (int i, float v) override
    {
        if (i == 0) freq.store (clamp (v, 20.0f, 800.0f));
        else mixP.store (clamp (v, 0.0f, 1.0f));
    }
private:
    std::atomic<float> freq { 140.0f }, mixP { 0.25f };
    float phase { 0 };
    double sampleRate { 44100 };
    inline static const std::vector<ParamDesc> descs {
        { "freq", "Freq", 20, 800, 140, " Hz" },
        { "mix", "Mix", 0, 1, 0.25f, "" }
    };
};

class BitcrushModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Bitcrush; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    void prepare (double, int, int) override { hold = 0; count = 0; }
    void reset() override { hold = 0; count = 0; }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int bits = juce::jlimit (4, 16, (int) std::round (bitsP.load()));
        const float rate = rateP.load(); // 0 = full sr, 1 = ~1/40
        const float mix = mixP.load();
        const int holdN = 1 + (int) (rate * 40.0f);
        const float steps = (float) ((1 << (bits - 1)) - 1);
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);
        for (int i = 0; i < n; ++i)
        {
            if (count <= 0)
            {
                hold = buffer.getSample (0, i);
                if (chs > 1) hold = 0.5f * (hold + buffer.getSample (1, i));
                hold = std::round (hold * steps) / steps;
                count = holdN;
            }
            --count;
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                buffer.setSample (c, i, lerp (x, hold, mix));
            }
        }
    }
    float getParam (int i) const override
    {
        if (i == 0) return bitsP.load();
        if (i == 1) return rateP.load();
        return mixP.load();
    }
    void setParam (int i, float v) override
    {
        if (i == 0) bitsP.store (clamp (v, 4.0f, 16.0f));
        else if (i == 1) rateP.store (clamp (v, 0.0f, 1.0f));
        else mixP.store (clamp (v, 0.0f, 1.0f));
    }
private:
    std::atomic<float> bitsP { 12 }, rateP { 0.15f }, mixP { 0.35f };
    float hold { 0 };
    int count { 0 };
    inline static const std::vector<ParamDesc> descs {
        { "bits", "Bits", 4, 16, 12, "", true },
        { "rate", "Down", 0, 1, 0.15f, "" },
        { "mix", "Mix", 0, 1, 0.35f, "" }
    };
};

} // namespace vc
