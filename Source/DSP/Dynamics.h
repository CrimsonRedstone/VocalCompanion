#pragma once
#include "Common.h"

namespace vc
{

//==============================================================================
/** Discrete FET compressor modelled on 1176 LN topology: ultra-fast attack,
    program-dependent release, All-Buttons (20:1 + extra grit). */
class FetCompModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::FetComp; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        reset();
    }

    void reset() override
    {
        env = 0;
        grMeter = 0;
        satStateL = satStateR = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float inGain  = dbToGain (inputDb.load());
        const float outGain = dbToGain (outputDb.load());
        const float thresh  = dbToGain (threshDb.load());
        float ratio = ratioP.load();
        const bool allButtons = ratio >= 19.5f;
        if (allButtons) ratio = 20.0f;

        const float atkMs = allButtons ? 0.02f : attackMs.load();
        float relMs = releaseMs.load();
        const float atkC = msToCoeff (atkMs, sampleRate);
        const float slope = 1.0f - 1.0f / std::max (ratio, 1.0f);
        const float mix = mixP.load();
        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), 2);
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < chs; ++c)
                peak = std::max (peak, std::abs (buffer.getSample (c, i) * inGain));

            // program-dependent release: faster on short peaks
            const float relScale = allButtons ? 0.4f : (0.35f + 0.65f * std::min (env / (thresh + 1.0e-6f), 2.0f));
            const float relC = msToCoeff (relMs / std::max (relScale, 0.25f), sampleRate);

            if (peak > env) env += atkC * (peak - env);
            else            env += relC * (peak - env);

            float gr = 1.0f;
            if (env > thresh)
            {
                const float overDb = gainToDb (env / thresh);
                gr = dbToGain (-overDb * slope);
            }

            // FET nonlinear conduction under heavy GR
            if (gr < 0.7f)
                gr *= 0.92f + 0.08f * fastTanh ((0.7f - gr) * 4.0f);

            maxGr = std::max (maxGr, 1.0f - gr);

            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                float y = x * inGain * gr;

                if (allButtons || gr < 0.5f)
                {
                    float& s = (c == 0 ? satStateL : satStateR);
                    s = 0.995f * s + 0.005f * y;
                    y = fastTanh (y * 1.15f + 0.12f * s);
                }

                y *= outGain;
                buffer.setSample (c, i, lerp (x, y, mix));
            }
        }

        grMeter.store (onePole (grMeter.load(), maxGr, 0.2f));
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return inputDb.load();
            case 1: return outputDb.load();
            case 2: return threshDb.load();
            case 3: return ratioP.load();
            case 4: return attackMs.load();
            case 5: return releaseMs.load();
            case 6: return mixP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: inputDb.store  (clamp (v, -24.0f, 24.0f)); break;
            case 1: outputDb.store (clamp (v, -24.0f, 24.0f)); break;
            case 2: threshDb.store (clamp (v, -40.0f, 0.0f)); break;
            case 3: ratioP.store   (clamp (v, 4.0f, 20.0f)); break;
            case 4: attackMs.store (clamp (v, 0.02f, 8.0f)); break;
            case 5: releaseMs.store(clamp (v, 50.0f, 1100.0f)); break;
            case 6: mixP.store     (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

    float getMeter (int) const override { return grMeter.load(); }
    int getNumMeters() const override { return 1; }

private:
    std::atomic<float> inputDb  { 0.0f };
    std::atomic<float> outputDb { 0.0f };
    std::atomic<float> threshDb { -18.0f };
    std::atomic<float> ratioP   { 8.0f };
    std::atomic<float> attackMs { 0.4f };
    std::atomic<float> releaseMs{ 200.0f };
    std::atomic<float> mixP     { 1.0f };
    std::atomic<float> grMeter  { 0.0f };

    double sampleRate { 44100.0 };
    float env { 0 }, satStateL { 0 }, satStateR { 0 };

    inline static const std::vector<ParamDesc> descs {
        { "input",   "Input",   -24.0f, 24.0f,   0.0f,  " dB" },
        { "output",  "Output",  -24.0f, 24.0f,   0.0f,  " dB" },
        { "thresh",  "Thresh",  -40.0f,  0.0f, -18.0f,  " dB" },
        { "ratio",   "Ratio",     4.0f, 20.0f,   8.0f,  ":1" },
        { "attack",  "Attack",    0.02f, 8.0f,   0.4f,  " ms" },
        { "release", "Release",  50.0f, 1100.0f, 200.0f," ms" },
        { "mix",     "Mix",       0.0f,  1.0f,   1.0f,  "" }
    };
};

//==============================================================================
/** Optical leveling amplifier — slow RMS, gentle knee, vocal glue. */
class OptoCompModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::OptoComp; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override { sampleRate = sr; reset(); }
    void reset() override { rms = 0; env = 0; grMeter = 0; }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float thresh = dbToGain (threshDb.load());
        const float ratio  = ratioP.load();
        const float peakW  = peakWeight.load();
        const float atkC   = msToCoeff (attackMs.load(), sampleRate);
        const float relC   = msToCoeff (releaseMs.load(), sampleRate);
        const float makeup = dbToGain (makeupDb.load());
        const float slope  = 1.0f - 1.0f / std::max (ratio, 1.0f);
        const float rmsC   = msToCoeff (8.0f, sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            float sq = 0, pk = 0;
            for (int c = 0; c < chs; ++c)
            {
                const float s = buffer.getSample (c, i);
                sq += s * s;
                pk = std::max (pk, std::abs (s));
            }
            sq = std::sqrt (sq / (float) std::max (chs, 1));
            rms += rmsC * (sq - rms);
            const float det = lerp (rms, pk, peakW);

            if (det > env) env += atkC * (det - env);
            else           env += relC * (det - env);

            float gr = 1.0f;
            if (env > thresh)
            {
                const float overDb = gainToDb (env / thresh);
                // soft knee 6 dB
                const float knee = 6.0f;
                float grDb = overDb < knee ? (overDb * overDb * slope) / (2.0f * knee)
                                           : (overDb - knee * 0.5f) * slope;
                gr = dbToGain (-grDb);
            }
            maxGr = std::max (maxGr, 1.0f - gr);

            for (int c = 0; c < chs; ++c)
                buffer.setSample (c, i, buffer.getSample (c, i) * gr * makeup);
        }
        grMeter.store (onePole (grMeter.load(), maxGr, 0.15f));
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return threshDb.load();
            case 1: return ratioP.load();
            case 2: return attackMs.load();
            case 3: return releaseMs.load();
            case 4: return makeupDb.load();
            case 5: return peakWeight.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: threshDb.store   (clamp (v, -40.0f, 0.0f)); break;
            case 1: ratioP.store     (clamp (v, 1.5f, 8.0f)); break;
            case 2: attackMs.store   (clamp (v, 5.0f, 80.0f)); break;
            case 3: releaseMs.store  (clamp (v, 80.0f, 2000.0f)); break;
            case 4: makeupDb.store   (clamp (v, 0.0f, 18.0f)); break;
            case 5: peakWeight.store (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

    float getMeter (int) const override { return grMeter.load(); }
    int getNumMeters() const override { return 1; }

private:
    std::atomic<float> threshDb   { -20.0f };
    std::atomic<float> ratioP     { 3.0f };
    std::atomic<float> attackMs   { 15.0f };
    std::atomic<float> releaseMs  { 280.0f };
    std::atomic<float> makeupDb   { 4.0f };
    std::atomic<float> peakWeight { 0.25f };
    std::atomic<float> grMeter    { 0.0f };
    double sampleRate { 44100.0 };
    float rms { 0 }, env { 0 };

    inline static const std::vector<ParamDesc> descs {
        { "thresh",  "Thresh",  -40.0f, 0.0f,   -20.0f, " dB" },
        { "ratio",   "Ratio",     1.5f, 8.0f,     3.0f, ":1" },
        { "attack",  "Attack",    5.0f, 80.0f,   15.0f, " ms" },
        { "release", "Release",  80.0f, 2000.0f, 280.0f," ms" },
        { "makeup",  "Makeup",    0.0f, 18.0f,    4.0f, " dB" },
        { "peak",    "Peak",      0.0f,  1.0f,    0.25f,"" }
    };
};

//==============================================================================
/** Dual-band vocal de-esser: synth harshness 2.5–4 kHz + sibilance 4–8 kHz. */
class DeEsserModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::DeEsser; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int chs) override
    {
        sampleRate = sr;
        numCh = std::min (chs, 2);
        for (int c = 0; c < 2; ++c)
        {
            harsh[c].setBandpass (3200.0f, 1.6f, sr);
            sib[c].setBandpass (6200.0f, 1.8f, sr);
        }
        reset();
    }

    void reset() override
    {
        envH = envS = 0;
        for (auto& f : harsh) f.reset();
        for (auto& f : sib) f.reset();
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const float thH = dbToGain (threshHarsh.load());
        const float thS = dbToGain (threshSib.load());
        const float rng = rangeDb.load();
        const float sens = 0.3f + 0.7f * senseP.load();
        const float atkC = msToCoeff (2.0f, sampleRate);
        const float relC = msToCoeff (40.0f + 80.0f * (1.0f - senseP.load()), sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), 2);
        float maxGr = 0, harshGr = 0, sibGr = 0;

        for (int i = 0; i < n; ++i)
        {
            float hDet = 0, sDet = 0;
            float hB[2] {}, sB[2] {};
            for (int c = 0; c < chs; ++c)
            {
                const float x = buffer.getSample (c, i);
                hB[c] = harsh[c].process (x);
                sB[c] = sib[c].process (x);
                hDet = std::max (hDet, std::abs (hB[c]));
                sDet = std::max (sDet, std::abs (sB[c]));
            }

            if (hDet > envH) envH += atkC * (hDet - envH); else envH += relC * (hDet - envH);
            if (sDet > envS) envS += atkC * (sDet - envS); else envS += relC * (sDet - envS);

            auto grFor = [&] (float env, float th)
            {
                if (env <= th) return 1.0f;
                const float over = gainToDb (env / th) * sens;
                return dbToGain (-clamp (over, 0.0f, rng));
            };
            const float gH = grFor (envH, thH);
            const float gS = grFor (envS, thS);
            harshGr = std::max (harshGr, 1.0f - gH);
            sibGr = std::max (sibGr, 1.0f - gS);
            maxGr = std::max (maxGr, 1.0f - std::min (gH, gS));

            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i);
                x += hB[c] * (gH - 1.0f);
                x += sB[c] * (gS - 1.0f);
                buffer.setSample (c, i, x);
            }
        }
        grMeter.store (onePole (grMeter.load(), maxGr, 0.25f));
        bandMeters[0].store (harshGr); bandMeters[1].store (sibGr);
    }

    float getParam (int i) const override
    {
        switch (i)
        {
            case 0: return threshHarsh.load();
            case 1: return threshSib.load();
            case 2: return rangeDb.load();
            case 3: return senseP.load();
            default: return 0;
        }
    }

    void setParam (int i, float v) override
    {
        switch (i)
        {
            case 0: threshHarsh.store (clamp (v, -48.0f, 0.0f)); break;
            case 1: threshSib.store   (clamp (v, -48.0f, 0.0f)); break;
            case 2: rangeDb.store     (clamp (v, 0.0f, 18.0f)); break;
            case 3: senseP.store      (clamp (v, 0.0f, 1.0f)); break;
            default: break;
        }
    }

    float getMeter (int i) const override { return i == 1 || i == 2 ? bandMeters[i-1].load() : grMeter.load(); }
    int getNumMeters() const override { return 3; }

private:
    std::atomic<float> bandMeters[2] {};
    std::atomic<float> threshHarsh { -22.0f };
    std::atomic<float> threshSib   { -18.0f };
    std::atomic<float> rangeDb     { 10.0f };
    std::atomic<float> senseP      { 0.7f };
    std::atomic<float> grMeter     { 0.0f };
    Biquad harsh[2], sib[2];
    double sampleRate { 44100.0 };
    int numCh { 2 };
    float envH { 0 }, envS { 0 };

    inline static const std::vector<ParamDesc> descs {
        { "harsh", "Harsh 3k", -48.0f, 0.0f, -22.0f, " dB" },
        { "sib",   "Sib 6k",   -48.0f, 0.0f, -18.0f, " dB" },
        { "range", "Range",      0.0f, 18.0f, 10.0f, " dB" },
        { "sense", "Sense",      0.0f,  1.0f,  0.7f, "" }
    };
};

//==============================================================================
/** Brickwall peak limiter used as the master safety net. */
class PeakLimiter
{
public:
    void prepare (double sr, int block)
    {
        sampleRate = sr;
        const int look = std::max (1, (int) std::round (0.0015 * sr)); // 1.5 ms
        lookahead = look;
        delay.setSize (2, look + block + 8);
        delay.clear();
        w = 0;
        env = 1;
        gainSm = 1;
    }

    void process (juce::AudioBuffer<float>& buffer, float ceilingDb, bool enabled, float releaseMs = 80.0f)
    {
        if (! enabled)
            return;

        const float ceilG = dbToGain (ceilingDb);
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);
        const int len = delay.getNumSamples();
        if (len <= lookahead + 1) return;
        const float relC = msToCoeff (releaseMs, sampleRate);
        const float atkC = msToCoeff (0.2f, sampleRate);

        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < chs; ++c)
            {
                const float x = buffer.getSample (c, i);
                delay.setSample (c, w, x);
                peak = std::max (peak, std::abs (x));
            }

            float needed = 1.0f;
            if (peak > ceilG)
                needed = ceilG / peak;

            if (needed < env) env += atkC * (needed - env);
            else              env += relC * (needed - env);
            gainSm = 0.7f * gainSm + 0.3f * env;

            const int r = (w - lookahead + len) % len;
            for (int c = 0; c < chs; ++c)
            {
                float y = delay.getSample (c, r) * gainSm;
                y = juce::jlimit (-ceilG, ceilG, y);
                buffer.setSample (c, i, y);
            }
            w = (w + 1) % len;
        }
        gr.store (1.0f - gainSm);
    }

    float getGr() const { return gr.load(); }
    int latencySamples() const { return lookahead; }

private:
    juce::AudioBuffer<float> delay;
    int w { 0 }, lookahead { 64 };
    double sampleRate { 44100.0 };
    float env { 1 }, gainSm { 1 };
    std::atomic<float> gr { 0 };
};

} // namespace vc

#include "VocalLimiter.h"
