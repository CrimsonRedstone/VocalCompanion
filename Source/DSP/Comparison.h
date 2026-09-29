#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

namespace vc
{
// Optional level matching between two settings snapshots. Only the selected
// settings are processed. Latency-aligned input is used for measurement only.
// Weighted energy matching is a listening aid, not an integrated LUFS meter.
class Comparison
{
public:
    static constexpr int chunkSize=256;
    std::atomic<int> mode{0}; // 0 settings A, 1 settings B
    std::atomic<bool> matchEnabled{true};
    std::atomic<float> matchDb{0};
    std::atomic<bool> ready{false},available{true};
    void prepare(double sr,int maximumLatency)
    {
        rate=sr;delay.setSize(2,std::max(1,maximumLatency+1));dry.setSize(2,chunkSize);
        for(int c=0;c<2;++c)for(int path=0;path<2;++path)
        {
            auto& w=weight[(size_t)path][(size_t)c];
            w.high.coefficients=juce::dsp::IIR::Coefficients<float>::makeHighPass(sr,60,.5f);
            w.shelf.coefficients=juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr,1500,.707f,1.584893f);
        }
        wetMix.reset(sr,.025);reset();
    }
    void reset()
    {
        delay.clear();dry.clear();position=age=0;lastLatency=-1;inEnergy.fill(0);outEnergy.fill(0);validSamples.fill(0);
        for(auto& path:weight)for(auto& w:path){w.high.reset();w.shelf.reset();}
        wetMix.setCurrentAndTargetValue(1);ready=false;matchDb=0;
    }
    void begin(const juce::AudioBuffer<float>& b,int latency)
    {
        const int length=delay.getNumSamples();
        available.store(length>latency&&latency>=0);
        if(!available.load())return;
        if(lastLatency!=latency){delay.clear();position=age=0;lastLatency=latency;}
        for(int i=0;i<b.getNumSamples();++i)
        {
            for(int c=0;c<std::min(2,b.getNumChannels());++c)
            {
                delay.setSample(c,position,b.getSample(c,i));
                dry.setSample(c,i,delay.getSample(c,(position-latency+length)%length));
            }
            position=(position+1)%length;age=std::min(age+1,length+chunkSize);
        }
    }
    void end(juce::AudioBuffer<float>& b)
    {
        if(!available.load())return;
        const int channels=std::min(2,b.getNumChannels()),n=b.getNumSamples();if(!channels||!n)return;
        double in=0,out=0;
        for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
        {
            auto& a=weight[0][(size_t)c];auto& z=weight[1][(size_t)c];
            const float x=a.high.processSample(a.shelf.processSample(dry.getSample(c,i)));
            const float y=z.high.processSample(z.shelf.processSample(b.getSample(c,i)));
            in+=x*x;out+=y*y;
        }
        in/=channels*n;out/=channels*n;
        const size_t slot=mode.load()==1?1u:0u;
        // Freeze estimates during silence and tails. Match B to A's measured
        // input/output ratio, so different source phrases influence it less.
        if(age>lastLatency&&in>1.e-8&&out>1.e-8)
        {
            const double a=1-std::exp(-n/(rate*.8));
            inEnergy[slot]+=a*(in-inEnergy[slot]);outEnergy[slot]+=a*(out-outEnergy[slot]);validSamples[slot]+=n;
        }
        const bool calibrated=validSamples[0]>rate*.25&&validSamples[1]>rate*.25;
        ready.store(calibrated);
        const float correction=calibrated&&slot==1?(float)juce::jlimit(-18.,18.,10*std::log10((inEnergy[1]+1.e-15)/(outEnergy[1]+1.e-15)*(outEnergy[0]+1.e-15)/(inEnergy[0]+1.e-15))):0.f;
        matchDb.store(correction);
        wetMix.setTargetValue(matchEnabled.load()?std::pow(10.f,correction*.05f):1.f);
        for(int i=0;i<n;++i)
        {
            const float gain=wetMix.getNextValue();
            if(gain==1.f)continue;
            for(int c=0;c<channels;++c)b.setSample(c,i,b.getSample(c,i)*gain);
        }
    }
private:
    struct Weight{juce::dsp::IIR::Filter<float> high,shelf;};
    std::array<std::array<Weight,2>,2> weight;
    juce::AudioBuffer<float> delay,dry;
    juce::SmoothedValue<float> wetMix;
    double rate=48000;std::array<double,2> inEnergy{},outEnergy{};std::array<long long,2> validSamples{};
    int position=0,age=0,lastLatency=-1;
};
}
