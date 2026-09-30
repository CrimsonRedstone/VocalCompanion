#pragma once
#include "Common.h"

namespace vc
{
// Conservative signal-based cleanup; no model downloads or audio recording.
class BreathControlModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::BreathControl; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }
    float getCoreParam(int i) const override { return i>=0&&i<6?p[(size_t)i].load():0; }
    void setCoreParam(int i,float v) override { if(i>=0&&i<6&&std::isfinite(v))p[(size_t)i].store(clamp(v,descs[(size_t)i].min,descs[(size_t)i].max)); }
    int latencySamples() const override { return 2; }
    void prepare(double rate,int,int) override { sr=rate;decimation=std::max(1,(int)(sr/12000));high.setHighpass(500,.707f,sr);reset(); }
    void reset() override { history.fill(0);for(auto& c:click)c.fill(0);position=hop=count=decimator=clickPos=0;sum=power=highPower=breathTime=0;gain=1;probability=0;high.reset();reduction=removed=0; }
    float getMeter(int i=0) const override { return i==1?removed.load():reduction.load(); }
    int getNumMeters() const override { return 2; }
    void process(juce::AudioBuffer<float>& b) override
    {
        const int channels=std::min(2,b.getNumChannels());if(channels==0)return;
        const float smooth=msToCoeff(20,sr),attack=msToCoeff(advanced(0),sr),release=msToCoeff(getParam(3),sr);
        const float sensitivity=getParam(1)*.01f, clickAmount=getParam(4)*.01f;
        float maxReduction=0,maxClick=0;
        for(int i=0;i<b.getNumSamples();++i)
        {
            // Select the stronger channel to avoid anti-phase cancellation in detection.
            float x=b.getSample(0,i);if(channels>1&&std::abs(b.getSample(1,i))>std::abs(x))x=b.getSample(1,i);
            const float h=high.process(x);power+=smooth*(x*x-power);highPower+=smooth*(h*h-highPower);
            sum+=x;
            if(++decimator>=decimation)
            {
                history[(size_t)position]=sum/decimation;position=(position+1)%512;sum=0;decimator=0;count=std::min(512,count+1);
                if(++hop>=128&&count==512)
                {
                    hop=0;float periodic=0;
                    const double analysisRate=sr/decimation;
                    for(int lag=std::max(8,(int)(analysisRate/1000));lag<std::min(240,(int)(analysisRate/65));lag+=2)
                    {
                        double cross=0,energy=1.e-12;
                        for(int j=0;j<512-lag;++j){float a=history[(size_t)((position+j)%512)],z=history[(size_t)((position+j+lag)%512)];cross+=a*z;energy+=a*a+z*z;}
                        periodic=std::max(periodic,(float)(2*cross/energy));
                    }
                    probability=clamp((.8f-periodic)*2,0,1)*clamp((highPower/(power+1.e-12f)-.12f)*2,0,1);
                }
            }
            const float db=gainToDb(std::sqrt(power));
            const bool breath=count==512&&db>-70&&db<getParam(2)&&probability>(.7f-.45f*sensitivity);
            breathTime=breath?breathTime+1.f/(float)sr:0;
            const float target=breathTime>advanced(1)*.001f?dbToGain(-getParam(0)*probability):1.f;
            gain+=(target<gain?attack:release)*(target-gain);
            for(int c=0;c<channels;++c)
            {
                auto& ring=click[(size_t)c];ring[(size_t)clickPos]=b.getSample(c,i);
                const float centre=ring[(size_t)((clickPos+3)%5)],before=ring[(size_t)((clickPos+2)%5)],after=ring[(size_t)((clickPos+4)%5)];
                const float estimate=.5f*(before+after);
                const bool spike=std::abs(centre)>3*std::max(std::abs(before),std::abs(after))+.015f && std::abs(centre-estimate)>.02f;
                const float clean=spike?lerp(centre,estimate,clickAmount):centre;
                const float result=clean*gain;maxClick=std::max(maxClick,std::abs(centre-clean));
                b.setSample(c,i,getParam(5)>.5f?centre-result:result);
            }
            clickPos=(clickPos+1)%5;maxReduction=std::max(maxReduction,1-gain);
        }
        reduction.store(maxReduction);removed.store(maxClick);
    }
private:
    double sr=48000;int position=0,hop=0,count=0,decimator=0,decimation=4,clickPos=0;
    std::array<float,512> history{};std::array<std::array<float,5>,2> click{};
    Biquad high;float sum=0,power=0,highPower=0,breathTime=0,gain=1,probability=0;
    std::atomic<float> reduction{0},removed{0};
    std::array<std::atomic<float>,6> p{{12,50,-18,100,50,0}};
    inline static const std::vector<ParamDesc> descs{
        {"reduction","Reduction",0,30,12," dB"},{"sense","Sensitivity",0,100,50,"%"},
        {"ceiling","Breath ceiling",-50,-6,-18," dB"},{"release","Release",20,500,100," ms"},
        {"clicks","Click repair",0,100,50,"%"},{"listen","Listen removed",0,1,0,"",true,{"Off","On"}}};
};

class VocalRiderModule final : public VcModule
{
public:
    ModuleType getType() const override { return ModuleType::VocalRider; }
    const std::vector<ParamDesc>& getCoreParamDescs() const override { return descs; }
    float getCoreParam(int i) const override { return i>=0&&i<6?p[(size_t)i].load():0; }
    void setCoreParam(int i,float v) override {if(i>=0&&i<6&&std::isfinite(v))p[(size_t)i].store(clamp(v,descs[(size_t)i].min,descs[(size_t)i].max));}
    void prepare(double rate,int,int) override {sr=rate;reset();}
    void reset() override {power=ride=0;quiet=0;meter=0;}
    float getMeter(int=0) const override {return meter.load();}
    int getNumMeters() const override {return 1;}
    void process(juce::AudioBuffer<float>& b) override
    {
        const float rms=msToCoeff(advanced(0),sr),speed=msToCoeff(getParam(3),sr),relax=msToCoeff(advanced(1),sr);
        const float gate=dbToGain(getParam(4));
        for(int i=0;i<b.getNumSamples();++i)
        {
            float peak=0;for(int c=0;c<b.getNumChannels();++c)peak=std::max(peak,std::abs(b.getSample(c,i)));
            power+=rms*(peak*peak-power);const float level=std::sqrt(power);
            quiet=peak<gate?std::min(quiet+1,(int)sr):0;
            const bool voiced=level>gate&&quiet<sr*.05;
            const float desired=voiced?clamp(getParam(0)-gainToDb(level),-getParam(2),getParam(1)):0;
            ride+=(voiced?speed:relax)*(desired-ride);
            const float gain=dbToGain(ride+getParam(5));
            for(int c=0;c<b.getNumChannels();++c)b.setSample(c,i,b.getSample(c,i)*gain);
        }
        meter.store(ride);
    }
private:
    double sr=48000;int quiet=0;float power=0,ride=0;std::atomic<float> meter{0};
    std::array<std::atomic<float>,6> p{{-18,6,12,350,-45,0}};
    inline static const std::vector<ParamDesc> descs{
        {"target","Target RMS",-36,-6,-18," dB"},{"boost","Max boost",0,18,6," dB"},{"cut","Max cut",0,24,12," dB"},
        {"speed","Ride time",80,2000,350," ms"},{"gate","Noise floor",-70,-20,-45," dB"},{"output","Output",-12,12,0," dB"}};
};

class PlosiveControlModule final : public VcModule
{
public:
    ModuleType getType() const override {return ModuleType::PlosiveControl;}
    const std::vector<ParamDesc>& getCoreParamDescs() const override {return descs;}
    float getCoreParam(int i) const override {return i>=0&&i<6?p[(size_t)i].load():0;}
    void setCoreParam(int i,float v) override {if(i>=0&&i<6&&std::isfinite(v))p[(size_t)i].store(clamp(v,descs[(size_t)i].min,descs[(size_t)i].max));}
    void prepare(double rate,int,int) override {sr=rate;reset();}
    void reset() override {for(auto& f:low)f.reset();energy=baseline=full=amount=0;hold=0;meter=0;}
    float getMeter(int=0) const override {return meter.load();}
    int getNumMeters() const override {return 1;}
    void process(juce::AudioBuffer<float>& b) override
    {
        for(auto& f:low)f.setLowpass(getParam(1),.707f,sr);
        const float fast=msToCoeff(3,sr),slow=msToCoeff(180,sr),attack=msToCoeff(advanced(0),sr),release=msToCoeff(getParam(4),sr);
        float maxReduction=0;
        for(int i=0;i<b.getNumSamples();++i)
        {
            float lows[2]{},lowPeak=0,peak=0;
            for(int c=0;c<std::min(2,b.getNumChannels());++c){const float x=b.getSample(c,i);lows[c]=low[(size_t)c].process(x);lowPeak=std::max(lowPeak,std::abs(lows[c]));peak=std::max(peak,std::abs(x));}
            energy+=fast*(lowPeak*lowPeak-energy);full+=fast*(peak*peak-full);
            const bool burst=gainToDb(std::sqrt(energy))>getParam(2)&&energy>baseline*dbToGain(getParam(3)*2)&&energy>advanced(2)*full;
            baseline+=slow*(energy-baseline);
            if(burst)hold=(int)(sr*advanced(1)*.001);else if(hold>0)--hold;
            const float target=hold>0?1-dbToGain(-getParam(0)):0;
            amount+=(target>amount?attack:release)*(target-amount);maxReduction=std::max(maxReduction,amount);
            for(int c=0;c<std::min(2,b.getNumChannels());++c){const float removed=lows[c]*amount;b.setSample(c,i,getParam(5)>.5f?removed:b.getSample(c,i)-removed);}
        }
        meter.store(maxReduction);
    }
private:
    double sr=48000;std::array<Biquad,2> low;float energy=0,baseline=0,full=0,amount=0;int hold=0;std::atomic<float> meter{0};
    std::array<std::atomic<float>,6> p{{12,150,-24,6,120,0}};
    inline static const std::vector<ParamDesc> descs{
        {"reduction","Reduction",0,30,12," dB"},{"frequency","Low band",60,300,150," Hz"},
        {"threshold","Threshold",-50,-6,-24," dB"},{"burst","Burst rise",2,18,6," dB"},
        {"release","Release",30,500,120," ms"},{"listen","Monitor",0,1,0,"",true,{"Processed","Removed only"}}};
};
}
