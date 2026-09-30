#pragma once
#include "Common.h"

namespace vc
{
/** Two deliberately different gain structures. Tube uses cascaded biased soft
    clipping with sag; Rat uses a frequency-shaped high-gain stage, slew limiting
    and symmetric diode clipping followed by the pedal-style darkening filter.
    These are inspired models, not claims of component-exact hardware emulation. */
class DriveModule final : public VcModule
{
public:
    explicit DriveModule(bool ratStyle) : rat(ratStyle) {}
    ModuleType getType() const override { return rat?ModuleType::RatDistortion:ModuleType::TubeOverdrive; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override {return rat?ratDescs:tubeDescs;}
    float getCoreParam(int i) const override {return i>=0&&i<5?p[(size_t)i].load():0;}
    void setCoreParam(int i,float v) override {if(i>=0&&i<5&&std::isfinite(v)){const auto& d=getParamDescs()[(size_t)i];p[(size_t)i]=clamp(v,d.min,d.max);}}
    void prepare(double rate,int block,int channels) override
    {
        sr=rate;os=std::make_unique<juce::dsp::Oversampling<float>>(juce::jmin(2,channels),1,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
        os->initProcessing((size_t)juce::jmax(block,Comparison::chunkSize));
        delay.setSize(2,128);reset();
    }
    int latencySamples() const override {return os?(int)std::round(os->getLatencyInSamples()):0;}
    void reset() override {if(os)os->reset();for(auto& f:inputHp)f.reset();for(auto& f:tone)f.reset();for(auto& f:dc)f.reset();envelope.fill(0);slew.fill(0);delay.clear();pos=0;}
    void process(juce::AudioBuffer<float>& buffer) override
    {
        if(!os)return;
        if(buffer.getNumSamples()>Comparison::chunkSize)
        {
            for(int start=0;start<buffer.getNumSamples();start+=Comparison::chunkSize)
            {float* ptr[2]{buffer.getWritePointer(0,start),buffer.getNumChannels()>1?buffer.getWritePointer(1,start):nullptr};juce::AudioBuffer<float> part(ptr,juce::jmin(2,buffer.getNumChannels()),juce::jmin(Comparison::chunkSize,buffer.getNumSamples()-start));process(part);}
            return;
        }
        const int channels=juce::jmin(2,buffer.getNumChannels()),n=buffer.getNumSamples(),latency=latencySamples();
        float dry[2][Comparison::chunkSize]{};
        // Chain feeds <=256-sample chunks. Align the dry branch with oversampling.
        jassert(n<=Comparison::chunkSize);
        for(int i=0;i<n;++i){for(int c=0;c<channels;++c){delay.setSample(c,pos,buffer.getSample(c,i));dry[c][i]=delay.getSample(c,(pos-latency+128)%128);}pos=(pos+1)%128;}
        juce::dsp::AudioBlock<float> block(buffer);auto up=os->processSamplesUp(block);
        const float gain=dbToGain(getParam(0)),colour=getParam(1)/100,shape=getParam(4)/100;
        const double rate=sr*2;
        for(int c=0;c<channels;++c)
        {
            inputHp[c].setHighpass(advanced(0),.707f,rate);
            // RAT Filter turns clockwise toward darker tones, like the pedal.
            const float cutoff=rat?18000.f*std::pow(500.f/18000.f,colour):700.f*std::pow(16000.f/700.f,colour);
            tone[c].setLowpass(std::min(cutoff,(float)rate*.45f),.707f,rate);dc[c].setHighpass(12,.707f,rate);
            auto* data=up.getChannelPointer((size_t)c);
            for(size_t i=0;i<up.getNumSamples();++i)
            {
                float x=inputHp[c].process(data[i]);
                envelope[c]+=msToCoeff(40,rate)*(std::abs(x)-envelope[c]);
                if(rat)
                {
                    const float amplified=x*gain*8;
                    const float step=(float)(9000/rate)*(1-.85f*shape);
                    slew[c]+=clamp(amplified-slew[c],-step,step);
                    x=clamp(slew[c],-advanced(1),advanced(1))/advanced(1);
                }
                else
                {
                    const float sag=1/(1+shape*envelope[c]*3),bias=.15f+.25f*shape+advanced(1);
                    x=std::tanh(x*gain*sag+bias)-std::tanh(bias);
                    x=std::tanh(x*1.8f-.1f)+std::tanh(.1f);
                }
                data[i]=dc[c].process(tone[c].process(x));
            }
        }
        os->processSamplesDown(block);
        const float mix=getParam(3)/100,out=dbToGain(getParam(2));
        for(int c=0;c<channels;++c)for(int i=0;i<n;++i)buffer.setSample(c,i,dry[c][i]*(1-mix)+buffer.getSample(c,i)*out*mix);
    }
private:
    bool rat;double sr=48000;int pos=0;
    std::array<std::atomic<float>,5> p{{18,50,-12,100,30}};
    std::array<float,2> envelope{},slew{};
    Biquad inputHp[2],tone[2],dc[2];juce::AudioBuffer<float> delay;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    inline static const std::vector<ParamDesc> tubeDescs{
        {"drive","Drive",0,48,18," dB"},{"tone","Tone",0,100,50,"%"},{"output","Output",-36,12,-12," dB"},{"mix","Mix",0,100,100,"%"},{"sag","Sag",0,100,30,"%"}};
    inline static const std::vector<ParamDesc> ratDescs{
        {"drive","Distortion",0,48,18," dB"},{"tone","Filter",0,100,50,"%"},{"output","Output",-36,12,-12," dB"},{"mix","Mix",0,100,100,"%"},{"slew","Slew",0,100,30,"%"}};
};
}
