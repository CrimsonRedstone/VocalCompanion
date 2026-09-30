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
    SoftTension = 0,  // Variable rational curve tension
    HardClip,         // Brickwall limiter / hard distortion
    TubeAsymmetric,   // Asymmetric curve introducing warm 2nd-order even harmonics
    SineFold,         // Wavefolder (FM / metallic timbre)
    Custom = 5,      // Editable point curve
    TapeSaturate = 4      // Smooth polynomial tape drive
};

struct ShapePoint { float x=0,y=0,tension=0; int curve=0; };
// A segment inherits its shape from the right-hand point, matching envelope editors.
inline float pointCurve(float x,const ShapePoint* pts,int count)
{
    if(count<2)return x;
    if(x<=pts[0].x)return pts[0].y;
    for(int i=1;i<count;++i)if(x<=pts[i].x)
    {
        const auto& a=pts[i-1];const auto& b=pts[i];
        float t=clamp((x-a.x)/std::max(.0001f,b.x-a.x),0,1);
        const float power=std::pow(8.f,b.tension);
        if(b.curve==2)t=t<1?0:1;
        else if(b.curve==1)t=t<.5f?.5f*std::pow(t*2,power):1-.5f*std::pow((1-t)*2,power);
        else t=std::pow(t,power);
        return lerp(a.y,b.y,t);
    }
    return pts[count-1].y;
}

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
            channels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        dryDelay.setSize((int)channels,128);
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

        dryDelay.clear();delayPos=0;
        dcBlockerL = 0.0f;
        dcBlockerR = 0.0f;
        prevInL = 0.0f;
        prevInR = 0.0f;
    }

    int oversamplingLatency() const {return oversampling?(int)std::round(oversampling->getLatencyInSamples()):0;}
    void setCurve(const std::array<ShapePoint,32>& p,int n)
    {points=p;pointCount=n;rebuildLut();}
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
        if(mode.load()==WaveShaperMode::Custom)return pointCurve(x,points.data(),pointCount);
        return evaluateShape (x, tension.load(), bipolar.load(), mode.load());
    }

    static float evaluateShape (float x, float tension, bool bipolar, WaveShaperMode mode)
    {
        x = juce::jlimit(-1.0f, 1.0f, x);
        switch (mode)
        {
            case WaveShaperMode::SoftTension:
            {
                // Rational tension curve: (1 + k) * x / (1 + k * |x|)
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
        const int latency=useOversampling?oversamplingLatency():0;
        for(int i=0;i<numSamples;++i)
        {
            for(int ch=0;ch<numChannels;++ch)
            {
                dryDelay.setSample(ch,delayPos,buffer.getSample(ch,i));
                dryBufferStorage.setSample(ch,i,dryDelay.getSample(ch,(delayPos-latency+128)%128));
            }
            delayPos=(delayPos+1)%128;
        }

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
                    data[i] = sampleLut(data[i]+inputBias);
            }

            oversampling->processSamplesDown(block);
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = sampleLut(data[i]+inputBias);
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
    std::array<float,lutSize> lut{};
    std::array<ShapePoint,32> points{};int pointCount=0;
    juce::AudioBuffer<float> dryDelay;int delayPos=0;
    juce::AudioBuffer<float> dryBufferStorage;

    void rebuildLut()
    {

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

public:
    // Bias enters the nonlinear stage, while the dry branch remains untouched.
    void setAdvanced(float bias, float dcHz){inputBias=bias;dcAlpha=std::exp(-kTwoPi*dcHz/(float)sampleRate);}
private:
    float inputBias=0;
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
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }

    void prepare (double sr, int samplesPerBlock, int numChannels) override
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (samplesPerBlock, Comparison::chunkSize);
        spec.numChannels = (juce::uint32) numChannels;
        dsp.prepare (spec);
    }

    void reset() override { dsp.reset(); }
    int latencySamples() const override {return oversampling.load()>.5f?dsp.oversamplingLatency():0;}
    std::vector<ShapePoint> getPoints() const
    {const juce::ScopedLock lock(curveLock);return {points.begin(),points.begin()+pointCount};}
    void setPoints(std::vector<ShapePoint> updated)
    {
        if(updated.size()<2||updated.size()>32)return;
        for(auto& p:updated){if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.tension))return;p.x=clamp(p.x,-1,1);p.y=clamp(p.y,-1,1);p.tension=clamp(p.tension,-1,1);p.curve=juce::jlimit(0,2,p.curve);}
        std::sort(updated.begin(),updated.end(),[](auto a,auto b){return a.x<b.x;});
        updated.front().x=-1;updated.back().x=1;
        const juce::ScopedLock lock(curveLock);pointCount=(int)updated.size();
        std::copy(updated.begin(),updated.end(),points.begin());++curveVersion;mode=5;
    }
    juce::ValueTree toValueTree() const override
    {
        auto state=VcModule::toValueTree();juce::ValueTree curve("WAVE_POINTS");
        for(auto p:getPoints()){juce::ValueTree node("POINT");node.setProperty("x",p.x,nullptr);node.setProperty("y",p.y,nullptr);node.setProperty("tension",p.tension,nullptr);node.setProperty("curve",p.curve,nullptr);curve.appendChild(node,nullptr);}
        state.appendChild(curve,nullptr);return state;
    }
    void fromValueTree(const juce::ValueTree& state) override
    {
        VcModule::fromValueTree(state);const float savedMode=mode.load();std::vector<ShapePoint> restored;
        for(auto node:state.getChildWithName("WAVE_POINTS"))restored.push_back({(float)node.getProperty("x"),(float)node.getProperty("y"),(float)node.getProperty("tension"),(int)node.getProperty("curve")});
        if(restored.size()>=2)setPoints(restored);else setPoints({{-1,-1,0,0},{0,0,0,0},{1,1,0,0}});
        mode=savedMode;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        // UI curve edits are copied at a block boundary. If the editor owns the
        // lock, keep the previous curve for this block rather than blocking audio.
        if(renderedVersion!=curveVersion.load())
        {const juce::ScopedTryLock lock(curveLock);if(lock.isLocked()){dsp.setCurve(points,pointCount);renderedVersion=curveVersion.load();}}
        dsp.setParameters (preGainDb.load(), postGainDb.load(), mix.load(), tension.load(),
                           asym.load() > 0.5f, (WaveShaperMode) (int) mode.load(),
                           oversampling.load() > 0.5f);
        dsp.setAdvanced(advanced(0),advanced(1));
        dsp.process (buffer);
    }

    float getCoreParam (int index) const override
    {
        const std::atomic<float>* values[] = { &preGainDb, &postGainDb, &mix, &tension, &asym, &mode, &oversampling };
        return index >= 0 && index < 7 ? values[index]->load() : 0.0f;
    }

    void setCoreParam (int index, float value) override
    {
        switch (index)
        {
            case 0: preGainDb.store (juce::jlimit (-24.0f, 24.0f, value)); break;
            case 1: postGainDb.store (juce::jlimit (-24.0f, 24.0f, value)); break;
            case 2: mix.store (juce::jlimit (0.0f, 1.0f, value)); break;
            case 3: tension.store (juce::jlimit (0.0f, 1.0f, value)); break;
            case 4: asym.store (value >= 0.5f ? 1.0f : 0.0f); break;
            case 5: mode.store ((float) juce::jlimit (0, 5, (int) std::round (value))); break;
            case 6: oversampling.store (value >= 0.5f ? 1.0f : 0.0f); break;
            default: break;
        }
    }

    float evaluateShape (float x) const
    {
        if((int)mode.load()==5){const juce::ScopedLock lock(curveLock);return pointCurve(x,points.data(),pointCount);}
        return WaveShaperDSP::evaluateShape (x, tension.load(), asym.load() > 0.5f,
                                             (WaveShaperMode) (int) mode.load());
    }

private:
    mutable juce::CriticalSection curveLock;
    std::array<ShapePoint,32> points{{{-1,-1,0,0},{0,0,0,0},{1,1,0,0}}};
    int pointCount=3,renderedVersion=-1;std::atomic<int> curveVersion{0};
    WaveShaperDSP dsp;
    std::atomic<float> preGainDb { 0.0f }, postGainDb { 0.0f }, mix { 1.0f }, tension { 0.35f };
    std::atomic<float> asym { 0.0f }, mode { 0.0f }, oversampling { 1.0f };

    inline static const std::vector<ParamDesc> descs {
        { "pre", "Pre", -24, 24, 0, " dB" },
        { "post", "Post", -24, 24, 0, " dB" },
        { "mix", "Mix", 0, 1, 1, "" },
        { "tension", "Tension", 0, 1, 0.35f, "" },
        { "asym", "Asym", 0, 1, 0, "", true, { "Off", "On" } },
        { "mode", "Mode", 0, 5, 0, "", true, { "Soft", "Clip", "Tube", "Fold", "Tape", "Points" } },
        { "oversampling", "2x OS", 0, 1, 1, "", true, { "Off", "On" } }
    };
};

} // namespace vc