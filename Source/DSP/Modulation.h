#pragma once
#include "Common.h"

namespace vc
{

inline float lfoValue (float phase, int shape)
{
    switch (shape)
    {
        case 1: return phase < 0.5f ? 1.0f : -1.0f;                 // square
        case 2: return 1.0f - 4.0f * std::abs (phase - 0.5f);        // triangle
        case 3: return 2.0f * phase - 1.0f;                          // saw
        default: return std::sin (kTwoPi * phase);                   // sine
    }
}

inline float syncedHz (double bpm, int division)
{
    // 0=1/4 1=1/8 2=1/8t 3=1/16 4=1/2 5=1 bar 6=free (caller ignores)
    const float beats = bpm > 1.0 ? (float) bpm / 60.0f : 2.0f;
    switch (division)
    {
        case 1: return beats * 2.0f;
        case 2: return beats * 3.0f;
        case 3: return beats * 4.0f;
        case 4: return beats * 0.5f;
        case 5: return beats / 4.0f;
        default: return beats;
    }
}

//==============================================================================
class ChorusModule final : public VcModule
{
public:
    float getVisualPhase() const override { return shownPhase.load(); }
    ModuleType getType() const override { return ModuleType::Chorus; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        const int maxD = (int) (sr * 0.05) + 8;
        delay.setSize (2, maxD);
        delay.clear();
        w = 0;
        len = maxD;
        reset();
    }

    void reset() override
    {
        delay.clear();
        phase = 0;
        w = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float rate = rateHz.load();
        const float depthMs = depthP.load();
        const float fb = feedback.load();
        const float mix = mixP.load();
        const float voices = voicesP.load();
        const float delay0 = delayMs.load();
        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), 2);
        const float dp = rate / (float) sampleRate;

        for (int i = 0; i < n; ++i)
        {
            phase += dp;
            if (phase >= 1.0f) phase -= 1.0f;
            float wetL = 0, wetR = 0;
            const int nv = juce::jlimit (1, 4, (int) std::round (voices));
            for (int v = 0; v < nv; ++v)
            {
                const float ph = std::fmod (phase + (float) v / (float) nv, 1.0f);
                const float mod = 0.5f + 0.5f * std::sin (kTwoPi * ph);
                const float dMs = delay0 + depthMs * mod;
                const float dS = dMs * 0.001f * (float) sampleRate;
                auto read = [&] (int ch, float dist) -> float
                {
                    float rp = (float) w - dist;
                    while (rp < 0) rp += (float) len;
                    const int i0 = ((int) rp) % len;
                    const int i1 = (i0 + 1) % len;
                    const float f = rp - (float) (int) rp;
                    return lerp (delay.getSample (ch, i0), delay.getSample (ch, i1), f);
                };
                wetL += read (0, dS);
                wetR += read (1, dS + 3.0f); // slight R offset
            }
            wetL /= (float) nv;
            wetR /= (float) nv;

            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                float wet = (c == 0 ? wetL : wetR);
                float y = x + wet * fb * 0.3f;
                delay.setSample (c, w, y);
                buffer.setSample (c, i, lerp (x, 0.7f * x + 0.7f * wet, mix));
            }
            if (chs == 1)
                delay.setSample (1, w, delay.getSample (0, w));
            w = (w + 1) % len;
        }
        shownPhase.store (phase, std::memory_order_relaxed);
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return rateHz.load();
            case 1: return depthP.load();
            case 2: return delayMs.load();
            case 3: return voicesP.load();
            case 4: return feedback.load();
            case 5: return mixP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: rateHz.store  (clamp (v, 0.05f, 8.0f)); break;
            case 1: depthP.store  (clamp (v, 0.1f, 8.0f)); break;
            case 2: delayMs.store (clamp (v, 4.0f, 30.0f)); break;
            case 3: voicesP.store (clamp (v, 1.0f, 4.0f)); break;
            case 4: feedback.store(clamp (v, 0.0f, 0.9f)); break;
            case 5: mixP.store    (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> shownPhase { 0 };
    std::atomic<float> rateHz { 0.8f };
    std::atomic<float> depthP { 2.4f };
    std::atomic<float> delayMs { 12.0f };
    std::atomic<float> voicesP { 3.0f };
    std::atomic<float> feedback { 0.15f };
    std::atomic<float> mixP { 0.4f };
    juce::AudioBuffer<float> delay;
    int w { 0 }, len { 1 };
    float phase { 0 };
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "rate", "Rate", 0.05f, 8, 0.8f, " Hz" },
        { "depth", "Depth", 0.1f, 8, 2.4f, " ms" },
        { "delay", "Delay", 4, 30, 12, " ms" },
        { "voices", "Voices", 1, 4, 3, "", true },
        { "fb", "Feedback", 0, 0.9f, 0.15f, "" },
        { "mix", "Mix", 0, 1, 0.4f, "" }
    };
};

//==============================================================================
class PhaserModule final : public VcModule
{
public:
    float getVisualPhase() const override { return shownPhase.load(); }
    ModuleType getType() const override { return ModuleType::Phaser; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override { sampleRate = sr; reset(); }
    void reset() override
    {
        phase = 0;
        for (auto& s : ap) s = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float rate = rateHz.load();
        const float depth = depthP.load();
        const float fb = feedback.load();
        const float mix = mixP.load();
        const int stages = juce::jlimit (2, 8, (int) std::round (stagesP.load()));
        const float centre = centreHz.load();
        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), 2);
        const float dp = rate / (float) sampleRate;

        for (int i = 0; i < n; ++i)
        {
            phase += dp;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * phase);
            const float hz = centre * std::pow (2.0f, (lfo - 0.5f) * depth * 2.0f);
            const float wc = std::tan (kPi * juce::jlimit (40.0f, (float) sampleRate * 0.45f, hz) / (float) sampleRate);
            const float a = (1.0f - wc) / (1.0f + wc);

            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                float y = x + fbState[c] * fb;
                for (int s = 0; s < stages; ++s)
                {
                    const int idx = c * 8 + s;
                    const float in = y;
                    y = a * in + ap[idx];
                    ap[idx] = in - a * y;
                }
                fbState[c] = y;
                buffer.setSample (c, i, lerp (x, 0.5f * (x + y), mix));
            }
        }
        shownPhase.store (phase, std::memory_order_relaxed);
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return rateHz.load();
            case 1: return depthP.load();
            case 2: return centreHz.load();
            case 3: return stagesP.load();
            case 4: return feedback.load();
            case 5: return mixP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: rateHz.store   (clamp (v, 0.05f, 10.0f)); break;
            case 1: depthP.store   (clamp (v, 0.0f, 1.0f)); break;
            case 2: centreHz.store (clamp (v, 200.0f, 4000.0f)); break;
            case 3: stagesP.store  (clamp (v, 2.0f, 8.0f)); break;
            case 4: feedback.store (clamp (v, 0.0f, 0.85f)); break;
            case 5: mixP.store     (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> shownPhase { 0 };
    std::atomic<float> rateHz { 0.4f };
    std::atomic<float> depthP { 0.7f };
    std::atomic<float> centreHz { 900.0f };
    std::atomic<float> stagesP { 6.0f };
    std::atomic<float> feedback { 0.4f };
    std::atomic<float> mixP { 0.5f };
    float ap[16] {}, fbState[2] {}, phase { 0 };
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "rate", "Rate", 0.05f, 10, 0.4f, " Hz" },
        { "depth", "Depth", 0, 1, 0.7f, "" },
        { "centre", "Centre", 200, 4000, 900, " Hz" },
        { "stages", "Stages", 2, 8, 6, "", true },
        { "fb", "Feedback", 0, 0.85f, 0.4f, "" },
        { "mix", "Mix", 0, 1, 0.5f, "" }
    };
};

//==============================================================================
class TremoloModule final : public VcModule
{
public:
    float getVisualPhase() const override { return shownPhase.load(); }
    ModuleType getType() const override { return ModuleType::Tremolo; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override { sampleRate = sr; phase = 0; }

    void reset() override { phase = 0; }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const bool sync = syncP.load() > 0.5f;
        const float rate = sync ? syncedHz (currentBpm, (int) std::round (divP.load()))
                                : rateHz.load();
        const float depth = depthP.load();
        const int shape = (int) std::round (shapeP.load());
        const float dp = rate / (float) sampleRate;
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            phase += dp;
            if (phase >= 1.0f) phase -= 1.0f;
            const float l = 0.5f + 0.5f * lfoValue (phase, shape);
            const float g = 1.0f - depth * (1.0f - l);
            for (int c = 0; c < chs; ++c)
                buffer.setSample (c, i, buffer.getSample (c, i) * g);
        }
        shownPhase.store (phase, std::memory_order_relaxed);
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return rateHz.load();
            case 1: return depthP.load();
            case 2: return shapeP.load();
            case 3: return syncP.load();
            case 4: return divP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: rateHz.store (clamp (v, 0.1f, 20.0f)); break;
            case 1: depthP.store (clamp (v, 0.0f, 1.0f)); break;
            case 2: shapeP.store (clamp (v, 0.0f, 3.0f)); break;
            case 3: syncP.store  (clamp (v, 0.0f, 1.0f)); break;
            case 4: divP.store   (clamp (v, 0.0f, 5.0f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> shownPhase { 0 };
    std::atomic<float> rateHz { 4.0f };
    std::atomic<float> depthP { 0.55f };
    std::atomic<float> shapeP { 0.0f };
    std::atomic<float> syncP  { 0.0f };
    std::atomic<float> divP   { 0.0f };
    float phase { 0 };
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "rate", "Rate", 0.1f, 20, 4, " Hz" },
        { "depth", "Depth", 0, 1, 0.55f, "" },
        { "shape", "Shape", 0, 3, 0, "", true },
        { "sync", "Sync", 0, 1, 0, "", true },
        { "div", "Div", 0, 5, 0, "", true }
    };
};

//==============================================================================
class AutoPanModule final : public VcModule
{
public:
    float getVisualPhase() const override { return shownPhase.load(); }
    ModuleType getType() const override { return ModuleType::AutoPan; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override { sampleRate = sr; phase = 0; }
    void reset() override { phase = 0; }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (buffer.getNumChannels() < 2)
            return;
        const bool sync = syncP.load() > 0.5f;
        const float rate = sync ? syncedHz (currentBpm, (int) std::round (divP.load()))
                                : rateHz.load();
        const float width = widthP.load();
        const float dp = rate / (float) sampleRate;
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            phase += dp;
            if (phase >= 1.0f) phase -= 1.0f;
            const float pan = std::sin (kTwoPi * phase) * width; // -1..1
            const float al = std::cos ((pan + 1.0f) * kPi * 0.25f);
            const float ar = std::sin ((pan + 1.0f) * kPi * 0.25f);
            const float l = buffer.getSample (0, i);
            const float r = buffer.getSample (1, i);
            const float m = 0.5f * (l + r);
            buffer.setSample (0, i, m * al * 1.414f);
            buffer.setSample (1, i, m * ar * 1.414f);
        }
        shownPhase.store (phase, std::memory_order_relaxed);
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return rateHz.load();
            case 1: return widthP.load();
            case 2: return syncP.load();
            case 3: return divP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: rateHz.store (clamp (v, 0.05f, 12.0f)); break;
            case 1: widthP.store (clamp (v, 0.0f, 1.0f)); break;
            case 2: syncP.store  (clamp (v, 0.0f, 1.0f)); break;
            case 3: divP.store   (clamp (v, 0.0f, 5.0f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> shownPhase { 0 };
    std::atomic<float> rateHz { 0.6f };
    std::atomic<float> widthP { 0.8f };
    std::atomic<float> syncP  { 0.0f };
    std::atomic<float> divP   { 0.0f };
    float phase { 0 };
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "rate", "Rate", 0.05f, 12, 0.6f, " Hz" },
        { "width", "Width", 0, 1, 0.8f, "" },
        { "sync", "Sync", 0, 1, 0, "", true },
        { "div", "Div", 0, 5, 0, "", true }
    };
};

} // namespace vc
