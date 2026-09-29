#pragma once
#include "Common.h"

namespace vc
{

/** Multi-mode input gain: Pure (linear), Clean (tube warmth), Dirty (overdrive). */
class GainModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Gain; }

    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int, int) override
    {
        sampleRate = sr;
        gainSm.reset (sr, 0.02);
        gainSm.setCurrentAndTargetValue (dbToGain (gainDb.load()));
        dcL.reset();
        dcR.reset();
        dcL.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 20.0));
        dcR.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 20.0));
        reset();
    }

    void reset() override
    {
        dcL.reset();
        dcR.reset();
        satEnv = 0;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int mode = (int) std::round (modeP.load());
        gainSm.setTargetValue (dbToGain (gainDb.load()));
        const float drive = driveP.load();
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.getNextValue();
            for (int c = 0; c < chs; ++c)
            {
                float x = buffer.getSample (c, i) * g;

                if (mode == 1) // Clean — soft 2nd-harmonic warmth
                {
                    const float d = 1.0f + drive * 1.4f;
                    x = fastTanh (x * d) / fastTanh (d);
                    x = x + 0.04f * drive * x * x; // even harmonic
                }
                else if (mode >= 2) // Dirty — high-gain tube
                {
                    const float d = 1.5f + drive * 8.0f;
                    float y = x * d;
                    y = fastTanh (y) + 0.15f * fastTanh (y * 3.0f);
                    y *= 0.55f;
                    // even + odd mix
                    y += 0.08f * drive * y * y;
                    x = juce::jlimit (-1.2f, 1.2f, y);
                }

                buffer.setSample (c, i, x);
            }
        }

        if (chs > 0) dcL.processSamples (buffer.getWritePointer (0), n);
        if (chs > 1) dcR.processSamples (buffer.getWritePointer (1), n);

        float peak = 0;
        for (int c = 0; c < chs; ++c)
            peak = std::max (peak, buffer.getMagnitude (c, 0, n));
        meter.store (peak, std::memory_order_relaxed);
    }

    float getParam (int index) const override
    {
        switch (index)
        {
            case 0: return gainDb.load();
            case 1: return modeP.load();
            case 2: return driveP.load();
            default: return 0;
        }
    }

    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: gainDb.store (clamp (value, -24.0f, 24.0f)); break;
            case 1: modeP.store (clamp (value, 0.0f, 2.0f)); break;
            case 2: driveP.store (clamp (value, 0.0f, 1.0f)); break;
            default: break;
        }
    }

    float getMeter (int) const override { return meter.load(); }
    int getNumMeters() const override { return 1; }

private:
    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> modeP  { 1.0f }; // 0 pure, 1 clean, 2 dirty
    std::atomic<float> driveP { 0.35f };
    std::atomic<float> meter  { 0.0f };

    juce::SmoothedValue<float> gainSm;
    juce::IIRFilter dcL, dcR;
    double sampleRate { 44100.0 };
    float satEnv { 0 };

    inline static const std::vector<ParamDesc> descs {
        { "gain",  "Gain",  -24.0f, 24.0f, 0.0f,  " dB" },
        { "mode",  "Mode",   0.0f,   2.0f, 1.0f,  "", true, juce::StringArray { "Pure", "Clean", "Dirty" } },
        { "drive", "Drive",  0.0f,   1.0f, 0.35f, "" }
    };
};

} // namespace vc
