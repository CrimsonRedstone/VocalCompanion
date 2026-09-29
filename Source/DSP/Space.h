#pragma once
#include "Common.h"

namespace vc
{

//==============================================================================
/** BPM-sync stereo delay with ping-pong and HP/LP tone. */
class DelayModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Delay; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        const int maxS = (int) (sr * 2.5) + 8;
        buf.setSize (2, maxS);
        buf.clear();
        len = maxS;
        w = 0;
        reset();
    }

    void reset() override
    {
        buf.clear();
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const bool sync = syncP.load() > 0.5f;
        float timeSec;
        if (sync)
        {
            const float beats = noteToBeats (divP.load());
            timeSec = (float) (beats * 60.0 / std::max (currentBpm, 20.0));
        }
        else timeSec = timeMs.load() * 0.001f;

        const float delayS = juce::jlimit (1.0f, (float) len - 4.0f, timeSec * (float) sampleRate);
        const float fb = feedback.load();
        const float mix = mixP.load();
        const bool ping = pingP.load() > 0.5f;
        hpL.setHighpass (hpHz.load(), 0.707f, sampleRate);
        hpR.setHighpass (hpHz.load(), 0.707f, sampleRate);
        lpL.setLowpass  (lpHz.load(), 0.707f, sampleRate);
        lpR.setLowpass  (lpHz.load(), 0.707f, sampleRate);

        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), 2);

        auto readAt = [&] (int ch, float dist)
        {
            float rp = (float) w - dist;
            while (rp < 0) rp += (float) len;
            const int i0 = ((int) rp) % len;
            const int i1 = (i0 + 1) % len;
            const float f = rp - (float) (int) rp;
            return lerp (buf.getSample (ch, i0), buf.getSample (ch, i1), f);
        };

        for (int i = 0; i < n; ++i)
        {
            float inL = buffer.getSample (0, i);
            float inR = chs > 1 ? buffer.getSample (1, i) : inL;
            float dL = readAt (0, delayS);
            float dR = readAt (1, ping ? delayS * 0.5f : delayS);
            dL = lpL.process (hpL.process (dL));
            dR = lpR.process (hpR.process (dR));

            if (ping)
            {
                buf.setSample (0, w, inL + dR * fb);
                buf.setSample (1, w, inR + dL * fb);
            }
            else
            {
                buf.setSample (0, w, inL + dL * fb);
                buf.setSample (1, w, inR + dR * fb);
            }

            buffer.setSample (0, i, lerp (inL, inL + dL, mix));
            if (chs > 1)
                buffer.setSample (1, i, lerp (inR, inR + dR, mix));
            w = (w + 1) % len;
        }
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return timeMs.load();
            case 1: return feedback.load();
            case 2: return mixP.load();
            case 3: return syncP.load();
            case 4: return divP.load();
            case 5: return pingP.load();
            case 6: return hpHz.load();
            case 7: return lpHz.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: timeMs.store   (clamp (v, 1.0f, 2000.0f)); break;
            case 1: feedback.store (clamp (v, 0.0f, 0.95f)); break;
            case 2: mixP.store     (clamp (v, 0.0f, 1.0f)); break;
            case 3: syncP.store    (clamp (v, 0.0f, 1.0f)); break;
            case 4: divP.store     (clamp (v, 0.0f, 5.0f)); break;
            case 5: pingP.store    (clamp (v, 0.0f, 1.0f)); break;
            case 6: hpHz.store     (clamp (v, 20.0f, 2000.0f)); break;
            case 7: lpHz.store     (clamp (v, 1000.0f, 18000.0f)); break;
            default: break;
        }
    }

private:
    static float noteToBeats (float div)
    {
        switch ((int) std::round (div))
        {
            case 1: return 0.5f;   // 1/8
            case 2: return 1.0f / 3.0f;
            case 3: return 0.25f;  // 1/16
            case 4: return 2.0f;   // 1/2
            case 5: return 4.0f;   // bar
            default: return 1.0f;  // 1/4
        }
    }

    std::atomic<float> timeMs { 380.0f };
    std::atomic<float> feedback { 0.35f };
    std::atomic<float> mixP { 0.28f };
    std::atomic<float> syncP { 1.0f };
    std::atomic<float> divP { 0.0f };
    std::atomic<float> pingP { 1.0f };
    std::atomic<float> hpHz { 180.0f };
    std::atomic<float> lpHz { 6500.0f };

    juce::AudioBuffer<float> buf;
    int w { 0 }, len { 1 };
    Biquad hpL, hpR, lpL, lpR;
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "time", "Time", 1, 2000, 380, " ms" },
        { "fb", "Feedback", 0, 0.95f, 0.35f, "" },
        { "mix", "Mix", 0, 1, 0.28f, "" },
        { "sync", "Sync", 0, 1, 1, "", true },
        { "div", "Div", 0, 5, 0, "", true },
        { "ping", "Ping", 0, 1, 1, "", true },
        { "hp", "HP", 20, 2000, 180, " Hz" },
        { "lp", "LP", 1000, 18000, 6500, " Hz" }
    };
};

//==============================================================================
/** Freeverb-style tank with a sidechain envelope that ducks the wet while
    the vocal is present, then blooms in the gaps. */
class ReverbModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Reverb; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        verb.prepare (spec);
        verb.reset();
        env.prepare (sr, 8.0f, 180.0f);
        reset();
    }

    void reset() override
    {
        verb.reset();
        env.reset();
        duckGain = 1;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize  = sizeP.load();
        p.damping   = dampP.load();
        p.wetLevel  = 1.0f;
        p.dryLevel  = 0.0f;
        p.width     = widthP.load();
        p.freezeMode = 0.0f;
        verb.setParameters (p);

        juce::AudioBuffer<float> wet;
        wet.makeCopyOf (buffer);
        juce::dsp::AudioBlock<float> block (wet);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        verb.process (ctx);

        const float mix = mixP.load();
        const float duckAmt = duckP.load();
        const float atk = msToCoeff (6.0f, sampleRate);
        const float rel = msToCoeff (220.0f, sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < chs; ++c)
                peak = std::max (peak, std::abs (buffer.getSample (c, i)));
            const float e = env.process (peak);
            const float target = 1.0f - duckAmt * juce::jlimit (0.0f, 1.0f, e * 3.5f);
            if (target < duckGain) duckGain += atk * (target - duckGain);
            else                   duckGain += rel * (target - duckGain);

            for (int c = 0; c < chs; ++c)
            {
                const float d = buffer.getSample (c, i);
                const float w = wet.getSample (c, i) * duckGain;
                buffer.setSample (c, i, lerp (d, d * (1.0f - mix * 0.35f) + w * mix, 1.0f));
            }
        }
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return sizeP.load();
            case 1: return dampP.load();
            case 2: return mixP.load();
            case 3: return widthP.load();
            case 4: return duckP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: sizeP.store  (clamp (v, 0.05f, 1.0f)); break;
            case 1: dampP.store  (clamp (v, 0.0f, 1.0f)); break;
            case 2: mixP.store   (clamp (v, 0.0f, 1.0f)); break;
            case 3: widthP.store (clamp (v, 0.0f, 1.0f)); break;
            case 4: duckP.store  (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> sizeP  { 0.42f };
    std::atomic<float> dampP  { 0.45f };
    std::atomic<float> mixP   { 0.18f };
    std::atomic<float> widthP { 1.0f };
    std::atomic<float> duckP  { 0.7f };

    juce::dsp::Reverb verb;
    EnvelopeFollower env;
    float duckGain { 1 };
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "size", "Size", 0.05f, 1, 0.42f, "" },
        { "damp", "Damp", 0, 1, 0.45f, "" },
        { "mix", "Mix", 0, 1, 0.18f, "" },
        { "width", "Width", 0, 1, 1, "" },
        { "duck", "Duck", 0, 1, 0.7f, "" }
    };
};

} // namespace vc
