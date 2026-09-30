#pragma once
#include "Common.h"
#include "StereoWidth.h"

namespace vc
{

//==============================================================================
/** Vocal dynamic EQ: low / body / honk / presence + air shelf. Dynamics pull
    boosted bands back when they get loud. */
class DynamicEqModule final : public VcModule
{
public:
    float getVisualEqGain (int i) const override { return i < 4 ? shownGain[i].load() : getParam (i); }
    ModuleType getType() const override { return ModuleType::DynamicEq; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        rebuild();
        reset();
    }

    void reset() override
    {
        for (auto& b : bands)
            for (auto& f : b.filt)
                f.reset();
        for (auto& f : airF) f.reset();
        for (auto& f : airPk) f.reset();
        for (auto& f : airHp) f.reset();
        for (auto& e : env) e = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        rebuild();
        const float airG = airDb.load();
        const float dyn  = dynAmt.load();
        const float th   = dbToGain (advanced(0));
        const float atkC = msToCoeff (advanced(1), sampleRate);
        const float relC = msToCoeff (advanced(2), sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = juce::jmin (buffer.getNumChannels(), 2);

        for (int i = 0; i < n; ++i)
        {
            float x[2];
            for (int c = 0; c < chs; ++c)
                x[c] = buffer.getSample (c, i);

            for (int b = 0; b < 4; ++b)
            {
                float det = 0;
                for (int c = 0; c < chs; ++c)
                    det = juce::jmax (det, std::abs (bands[b].det[c].process (x[c])));
                if (det > env[b]) env[b] += atkC * (det - env[b]);
                else              env[b] += relC * (det - env[b]);

                float g = bands[b].gainDb;
                if (g > 0.05f && dyn > 0.001f && env[b] > th)
                {
                    const float over = clamp (gainToDb (env[b] / th) / 18.0f, 0.0f, 1.0f);
                    g *= 1.0f - dyn * over;
                }
                if (i == n - 1) shownGain[b].store (g, std::memory_order_relaxed);
                if (std::abs (g) > 0.05f)
                {
                    for (int c = 0; c < chs; ++c)
                    {
                        bands[b].filt[c].setPeak (bands[b].hz, bands[b].q, g, sampleRate);
                        x[c] = bands[b].filt[c].process (x[c]);
                    }
                }
            }

            if (std::abs (airG) > 0.001f)
            {
                const float k = (airG / 12.0f) * 0.45f;
                for (int c = 0; c < chs; ++c)
                {
                    float dry = x[c];
                    float y = airPk[c].process (airF[c].process (dry));
                    float hi = airHp[c].process (dry);
                    x[c] = y + fastTanh (hi * 2.2f) * k;
                }
            }

            for (int c = 0; c < chs; ++c)
                buffer.setSample (c, i, x[c]);
        }
    }

    float getCoreParam (int i) const override
    {
        switch (i)
        {
            case 0: return gain[0].load();
            case 1: return gain[1].load();
            case 2: return gain[2].load();
            case 3: return gain[3].load();
            case 4: return airDb.load();
            case 5: return dynAmt.load();
            case 6: case 7: case 8: case 9: return quality[i-6].load();
            default: return 0;
        }
    }

    void setCoreParam (int i, float v) override
    {
        switch (i)
        {
            case 0: gain[0].store (clamp (v, -18.0f, 18.0f)); break;
            case 1: gain[1].store (clamp (v, -18.0f, 18.0f)); break;
            case 2: gain[2].store (clamp (v, -18.0f, 18.0f)); break;
            case 3: gain[3].store (clamp (v, -18.0f, 18.0f)); break;
            case 4: airDb.store   (clamp (v, -12.0f, 12.0f)); break;
            case 5: dynAmt.store  (clamp (v, 0.0f, 1.0f)); break;
            case 6: case 7: case 8: case 9: quality[i-6].store(clamp(v,.2f,12.f)); break;
            default: break;
        }
    }

private:
    std::atomic<float> quality[4] {{.8f},{1.f},{1.1f},{1.2f}};
    std::atomic<float> shownGain[4] {};
    struct Band
    {
        Biquad filt[2], det[2];
        float hz { 1000 }, q { 1 }, gainDb { 0 };
    };

    void rebuild()
    {
        const float kHz[4] = { advanced(3), advanced(4), advanced(5), advanced(6) };
        // Match detector bandwidth to the audible EQ bandwidth.
        for (int b = 0; b < 4; ++b)
        {
            bands[b].hz = kHz[b];
            bands[b].q = quality[b].load();
            bands[b].gainDb = gain[b].load();
            for (int c = 0; c < 2; ++c)
                bands[b].det[c].setBandpass (kHz[b], bands[b].q, sampleRate);
        }
        for (int c = 0; c < 2; ++c)
        {
            airF[c].setHighShelf (6500.0f, airDb.load(), sampleRate);
            airPk[c].setPeak (11000.0f, 0.7f, airDb.load() * 0.7f, sampleRate);
            airHp[c].setHighpass (6000.0f, 0.7f, sampleRate);
        }
    }

    std::atomic<float> gain[4] { 0, 0, 0, -1.0f };
    std::atomic<float> airDb { 2.0f };
    std::atomic<float> dynAmt { 0.4f };

    Band bands[4];
    Biquad airF[2], airPk[2], airHp[2];
    float env[4] {};
    double sampleRate { 44100.0 };

    inline static const std::vector<ParamDesc> descs {
        { "g1",  "Low",  -18, 18, 0,    " dB" },
        { "g2",  "Body", -18, 18, 0,    " dB" },
        { "g3",  "Honk", -18, 18, 0,    " dB" },
        { "g4",  "Pres", -18, 18, -1,   " dB" },
        { "air", "Air",  -12, 12, 2,    " dB" },
        { "dyn", "Dyn",    0,  1, 0.4f, "" },
        {"q1","Low Q",.2f,12,.8f,""}, {"q2","Body Q",.2f,12,1,""},
        {"q3","Honk Q",.2f,12,1.1f,""}, {"q4","Pres Q",.2f,12,1.2f,""}
    };
};

//==============================================================================
/** Independent Mid / Side 3-band EQ. */
class MsEqModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::MsEq; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }
    void prepare (double sr, int, int) override { sampleRate = sr; reset(); }
    void reset() override
    {
        mLo.reset(); mMid.reset(); mHi.reset();
        sLo.reset(); sMid.reset(); sHi.reset();
    }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        mLo.setLowShelf (180.0f, mlo.load(), sampleRate);
        mMid.setPeak (advanced(0), advanced(2), mmid.load(), sampleRate);
        mHi.setHighShelf (6500.0f, mhi.load(), sampleRate);
        sLo.setLowShelf (180.0f, slo.load(), sampleRate);
        sMid.setPeak (advanced(1), advanced(2), smid.load(), sampleRate);
        sHi.setHighShelf (7000.0f, shi.load(), sampleRate);
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();
        if (chs < 1) return;
        for (int i = 0; i < n; ++i)
        {
            float l = buffer.getSample (0, i);
            float r = chs > 1 ? buffer.getSample (1, i) : l;
            float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r);
            mid = mHi.process (mMid.process (mLo.process (mid)));
            side = sHi.process (sMid.process (sLo.process (side)));
            buffer.setSample (0, i, mid + side);
            if (chs > 1) buffer.setSample (1, i, mid - side);
        }
    }
    float getCoreParam (int i) const override
    {
        switch (i)
        {
            case 0: return mlo.load(); case 1: return mmid.load(); case 2: return mhi.load();
            case 3: return slo.load(); case 4: return smid.load(); case 5: return shi.load();
            default: return 0;
        }
    }
    void setCoreParam (int i, float v) override
    {
        v = clamp (v, -12.0f, 12.0f);
        switch (i)
        {
            case 0: mlo.store (v); break; case 1: mmid.store (v); break; case 2: mhi.store (v); break;
            case 3: slo.store (v); break; case 4: smid.store (v); break; case 5: shi.store (v); break;
            default: break;
        }
    }
private:
    std::atomic<float> mlo { 0 }, mmid { 0 }, mhi { 0 }, slo { 0 }, smid { 0 }, shi { 1.5f };
    Biquad mLo, mMid, mHi, sLo, sMid, sHi;
    double sampleRate { 44100 };
    inline static const std::vector<ParamDesc> descs {
        { "mlo",  "Mid Lo",  -12, 12, 0, " dB" },
        { "mmid", "Mid Mid", -12, 12, 0, " dB" },
        { "mhi",  "Mid Air", -12, 12, 0, " dB" },
        { "slo",  "Side Lo", -12, 12, 0, " dB" },
        { "smid", "Side Mid",-12, 12, 0, " dB" },
        { "shi",  "Side Air",-12, 12, 1.5f, " dB" }
    };
};

} // namespace vc
