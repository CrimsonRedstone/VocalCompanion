#pragma once
#include "Common.h"

#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace vc
{

enum class WaveShaperMode
{
    SoftTension = 0,  // Variable curve tension (Fruity S-curve)
    HardClip,         // Brickwall limiter / hard distortion
    TubeAsymmetric,   // Asymmetric curve introducing warm 2nd-order even harmonics
    SineFold,         // Wavefolder (FM / metallic timbre)
    TapeSaturate      // Smooth polynomial tape drive
};

class WaveShaperDSP
{
public:
    WaveShaperDSP() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        channels = spec.numChannels;
        dryBufferStorage.setSize ((int) channels, (int) spec.maximumBlockSize, false, false, true);

        // Initialize 2x oversampling to prevent aliasing
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            channels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampling->initProcessing(spec.maximumBlockSize);

        // DC Blocker filter: high-pass ~10 Hz
        dcBlockerL = 0.0f;
        dcBlockerR = 0.0f;
        dcAlpha = 1.0f - (juce::MathConstants<float>::twoPi * 10.0f / (float)sampleRate);

        rebuildLut();
    }

    void reset()
    {
        if (oversampling)
            oversampling->reset();

        dcBlockerL = 0.0f;
        dcBlockerR = 0.0f;
        prevInL = 0.0f;
        prevInR = 0.0f;
    }

    void setParameters(float newPreGainDb,
                       float newPostGainDb,
                       float newMix,
                       float newTension,
                       bool newBipolar,
                       WaveShaperMode newMode,
                       bool enableOversampling)
    {
        preGain = juce::Decibels::decibelsToGain(newPreGainDb);
        postGain = juce::Decibels::decibelsToGain(newPostGainDb);
        mix = juce::jlimit(0.0f, 1.0f, newMix);
        useOversampling = enableOversampling;

        if (std::abs(tension - newTension) > 0.001f || bipolar != newBipolar || mode != newMode)
        {
            tension = newTension;
            bipolar = newBipolar;
            mode = newMode;
            rebuildLut();
        }
    }

    // Evaluate transfer function for UI plotting: input x in [-1.0, 1.0] -> y in [-1.0, 1.0]
    float evaluateShape(float x) const
    {
        x = juce::jlimit(-1.0f, 1.0f, x);
        switch (mode)
        {
            case WaveShaperMode::SoftTension:
            {
                // Fruity Waveshaper tension curve: (1 + k) * x / (1 + k * |x|)
                float k = tension * 4.0f;
                if (!bipolar)
                {
                    float sign = (x >= 0.0f) ? 1.0f : -1.0f;
                    float ax = std::abs(x);
                    return sign * ((1.0f + k) * ax / (1.0f + k * ax));
                }
                else
                {
                    // Asymmetrical tension (different curve for positive vs negative)
                    if (x >= 0.0f)
                        return (1.0f + k) * x / (1.0f + k * x);
                    else
                    {
                        float kNeg = -tension * 2.0f;
                        return (1.0f + kNeg) * x / (1.0f + std::abs(kNeg * x));
                    }
                }
            }

            case WaveShaperMode::HardClip:
            {
                return juce::jlimit(-1.0f, 1.0f, x * (1.0f + tension * 3.0f));
            }

            case WaveShaperMode::TubeAsymmetric:
            {
                // Generates strong 2nd harmonic
                if (x >= 0.0f)
                    return std::tanh(x * (1.0f + tension * 2.0f));
                else
                    return x / (1.0f + std::abs(x * 0.5f));
            }

            case WaveShaperMode::SineFold:
            {
                float freq = (1.0f + tension * 4.0f) * juce::MathConstants<float>::halfPi;
                return std::sin(x * freq);
            }

            case WaveShaperMode::TapeSaturate:
            {
                // Cubic soft saturator
                float ax = juce::jlimit(-1.5f, 1.5f, x * (1.0f + tension * 1.5f));
                return ax - (ax * ax * ax) / 3.0f;
            }

            default:
                return x;
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        if (numChannels == 0 || numSamples == 0)
            return;

        jassert (numChannels <= dryBufferStorage.getNumChannels()
                 && numSamples <= dryBufferStorage.getNumSamples());
        for (int ch = 0; ch < numChannels; ++ch)
            dryBufferStorage.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // Pre-Gain
        buffer.applyGain(preGain);

        if (useOversampling && oversampling)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp(block);

            const int osSamples = (int)oversampledBlock.getNumSamples();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = oversampledBlock.getChannelPointer(ch);
                for (int i = 0; i < osSamples; ++i)
                    data[i] = sampleLut(data[i]);
            }

            oversampling->processSamplesDown(block);
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = sampleLut(data[i]);
            }
        }

        // DC Blocker filter (essential after asymmetric wave shaping)
        applyDcBlocker(buffer);

        // Post-Gain & Dry/Wet Mix
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* dry = dryBufferStorage.getReadPointer(ch);
            float* wet = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float wetSample = wet[i] * postGain;
                wet[i] = (1.0f - mix) * dry[i] + mix * wetSample;
            }
        }
    }

private:
    static constexpr int lutSize = 2048;
    std::vector<float> lut;
    juce::AudioBuffer<float> dryBufferStorage;

    void rebuildLut()
    {
        lut.resize(lutSize);
        for (int i = 0; i < lutSize; ++i)
        {
            // Map index [0, lutSize - 1] to [-1.0, 1.0]
            float norm = (float)i / (float)(lutSize - 1);
            float inVal = norm * 2.0f - 1.0f;
            lut[i] = evaluateShape(inVal);
        }
    }

    inline float sampleLut(float x) const
    {
        // Clamp to [-1.0, 1.0]
        float clamped = juce::jlimit(-1.0f, 1.0f, x);
        float norm = (clamped + 1.0f) * 0.5f * (float)(lutSize - 1);
        int idx = (int)norm;
        float frac = norm - (float)idx;

        if (idx >= lutSize - 1)
            return lut[lutSize - 1];

        // Linear interpolation
        return lut[idx] + frac * (lut[idx + 1] - lut[idx]);
    }

    void applyDcBlocker(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (buffer.getNumChannels() > 0)
        {
            float* l = buffer.getWritePointer(0);
            for (int i = 0; i < numSamples; ++i)
            {
                float in = l[i];
                float out = in - prevInL + dcAlpha * dcBlockerL;
                prevInL = in;
                dcBlockerL = out;
                l[i] = out;
            }
        }
        if (buffer.getNumChannels() > 1)
        {
            float* r = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                float in = r[i];
                float out = in - prevInR + dcAlpha * dcBlockerR;
                prevInR = in;
                dcBlockerR = out;
                r[i] = out;
            }
        }
    }

    double sampleRate = 44100.0;
    int channels = 2;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    float preGain = 1.0f;
    float postGain = 1.0f;
    float mix = 1.0f;
    std::atomic<float> tension { 0.0f };
    std::atomic<bool> bipolar { false };
    std::atomic<WaveShaperMode> mode { WaveShaperMode::SoftTension };
    bool useOversampling = true;

    // DC Blocker state
    float dcAlpha = 0.995f;
    float dcBlockerL = 0.0f, dcBlockerR = 0.0f;
    float prevInL = 0.0f, prevInR = 0.0f;
};

class WaveShaperModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::WaveShaper; }
    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }

    void prepare (double sr, int samplesPerBlock, int numChannels) override
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (samplesPerBlock, Comparison::chunkSize);
        spec.numChannels = (juce::uint32) numChannels;
        dsp.prepare (spec);
    }

    void reset() override { dsp.reset(); }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        dsp.setParameters (preGainDb.load(), postGainDb.load(), mix.load(), tension.load(),
                           asym.load() > 0.5f, (WaveShaperMode) (int) mode.load(),
                           oversampling.load() > 0.5f);
        dsp.process (buffer);
    }

    float getParam (int index) const override
    {
        const std::atomic<float>* values[] = { &preGainDb, &postGainDb, &mix, &tension, &asym, &mode, &oversampling };
        return index >= 0 && index < 7 ? values[index]->load() : 0.0f;
    }

    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: preGainDb.store (juce::jlimit (-24.0f, 24.0f, value)); break;
            case 1: postGainDb.store (juce::jlimit (-24.0f, 24.0f, value)); break;
            case 2: mix.store (juce::jlimit (0.0f, 1.0f, value)); break;
            case 3: tension.store (juce::jlimit (0.0f, 1.0f, value)); break;
            case 4: asym.store (value >= 0.5f ? 1.0f : 0.0f); break;
            case 5: mode.store ((float) juce::jlimit (0, 4, (int) std::round (value))); break;
            case 6: oversampling.store (value >= 0.5f ? 1.0f : 0.0f); break;
            default: break;
        }
    }

    const WaveShaperDSP& getDsp() const { return dsp; }

private:
    WaveShaperDSP dsp;
    std::atomic<float> preGainDb { 0.0f }, postGainDb { 0.0f }, mix { 1.0f }, tension { 0.35f };
    std::atomic<float> asym { 0.0f }, mode { 0.0f }, oversampling { 1.0f };

    inline static const std::vector<ParamDesc> descs {
        { "pre", "Pre", -24, 24, 0, " dB" },
        { "post", "Post", -24, 24, 0, " dB" },
        { "mix", "Mix", 0, 1, 1, "" },
        { "tension", "Tension", 0, 1, 0.35f, "" },
        { "asym", "Asym", 0, 1, 0, "", true, { "Off", "On" } },
        { "mode", "Mode", 0, 4, 0, "", true, { "Soft", "Clip", "Tube", "Fold", "Tape" } },
        { "oversampling", "2x OS", 0, 1, 1, "", true, { "Off", "On" } }
    };
};

} // namespace vc