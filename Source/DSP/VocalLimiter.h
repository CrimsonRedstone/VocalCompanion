#pragma once
#include "Common.h"

namespace vc
{
// Stereo-linked look-ahead peak limiting. Kept separate from the existing
// master safety limiter: the module has its own user-adjustable envelope.
class LimiterModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::Limiter; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }
    int latencySamples() const override { return (int)std::round(rate.load()*getParam(3)*.001); }
    void prepare (double sr, int, int) override
    {
        rate.store(sr);
        delay.setSize(2,(int)std::ceil(sr*.031)+2);
        inputGain.reset(sr,.01);
        inputGain.setCurrentAndTargetValue(dbToGain(getParam(2)));
        reset();
    }
    void reset() override
    {
        delay.clear(); position=holdSamples=0; envelope=target=1; power=0;
        reduction.store(0); detector.store(0);
        inputGain.setCurrentAndTargetValue(dbToGain(getParam(2)));
    }
    void process (juce::AudioBuffer<float>& buffer) override
    {
        if (delay.getNumSamples()==0) return;
        const auto sr=rate.load();
        const int ahead=latencySamples(), length=delay.getNumSamples();
        const float ceiling=dbToGain(getParam(0));
        const float attack=getParam(3), sustain=getParam(4), curve=getParam(5);
        const float attackC=msToCoeff(attack/(9.f-curve),sr);
        const float releaseC=msToCoeff(getParam(1)*advanced(1)*(.4f+curve*.15f),sr);
        const float rmsC=msToCoeff(sustain,sr);
        const float saturation=getParam(6), knee=dbToGain(saturation);
        const int channels=std::min(2,buffer.getNumChannels());
        float maxReduction=0;
        inputGain.setTargetValue(dbToGain(getParam(2)));
        for(int i=0;i<buffer.getNumSamples();++i)
        {
            const float gain=inputGain.getNextValue();
            float peak=0;
            for(int c=0;c<channels;++c)
            {
                const float x=buffer.getSample(c,i)*gain;
                delay.setSample(c,position,x); peak=std::max(peak,std::abs(x));
            }
            power += rmsC*(peak*peak-power);
            const float detect=std::max(peak,std::sqrt(std::max(0.f,power)));
            const float required=detect>ceiling?ceiling/detect:1.f;
            if(required<target){target=required;holdSamples=ahead+(int)(sr*advanced(0)*.001);}
            else if(holdSamples>0)--holdSamples;
            else target+=releaseC*(required-target);
            envelope+=(target<envelope?attackC:releaseC)*(target-envelope);
            const int read=(position-ahead+length)%length;
            float delayedPeak=0;
            for(int c=0;c<channels;++c)delayedPeak=std::max(delayedPeak,std::abs(delay.getSample(c,read)));
            // Linked safety gain, not independent L/R clipping. This also catches
            // single-sample peaks when Attack is deliberately set to zero.
            const float applied=std::min(envelope,delayedPeak>ceiling?ceiling/delayedPeak:1.f);
            maxReduction=std::max(maxReduction,1-applied);
            for(int c=0;c<channels;++c)
            {
                float y=delay.getSample(c,read)*applied;
                if(knee<ceiling && std::abs(y)>knee)
                {
                    const float room=std::max(1.e-7f,ceiling-knee);
                    y=std::copysign(knee+room*std::tanh((std::abs(y)-knee)/room),y);
                }
                buffer.setSample(c,i,y);
            }
            position=(position+1)%length;
        }
        reduction.store(maxReduction); detector.store(std::sqrt(std::max(0.f,power)));
    }
    float getCoreParam (int i) const override { return i>=0 && i<7 ? parameters[(size_t)i].load() : 0; }
    void setCoreParam (int i,float value) override
    {
        if(i<0 || i>=7 || !std::isfinite(value))return;
        const auto& d=descs[(size_t)i];
        parameters[(size_t)i].store(clamp(d.integer?std::round(value):value,d.min,d.max));
    }
    float getMeter (int i) const override { return i==1?detector.load():reduction.load(); }
    int getNumMeters() const override { return 2; }
    juce::ValueTree toValueTree() const override
    {
        auto t=VcModule::toValueTree();t.setProperty("limiterVersion",2,nullptr);return t;
    }
    void fromValueTree (const juce::ValueTree& t) override
    {
        VcModule::fromValueTree(t);
        // Old Makeup was after limiting. Shift both gain and ceiling to retain
        // that level relationship when restoring a pre-1.0.17 saved rack.
        if(!t.hasProperty("limiterVersion"))setParam(0,getParam(0)+getParam(2));
    }
private:
    juce::AudioBuffer<float> delay;
    juce::SmoothedValue<float> inputGain;
    std::atomic<double> rate {44100};
    int position=0,holdSamples=0;
    float envelope=1,target=1,power=0;
    std::atomic<float> reduction {0},detector {0};
    std::array<std::atomic<float>,7> parameters {{-.3f,80.f,0.f,1.5f,0.f,3.f,12.f}};
    inline static const std::vector<ParamDesc> descs {
        {"ceil","Ceiling",-30,12,-.3f," dB"},
        {"rel","Release",1,2000,80," ms"},
        {"makeup","Gain",-24,24,0," dB"},
        {"attack","Attack",0,30,1.5f," ms"},
        {"sustain","Sustain",0,1000,0," ms"},
        {"curve","Curve",1,8,3,"",true},
        {"sat","Saturation",-30,12,12," dB"}
    };
};
}
