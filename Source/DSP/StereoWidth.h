#pragma once
#include "Common.h"
namespace vc
{
/** Symmetric quadrature widener. Antisymmetric FIR kernels maintain quadrature
    in both crossover bands; adding/subtracting the same side signal avoids
    the former unequal dry/allpass interference in left and right channels. */
class ImagerModule final : public VcModule
{
public:
    ModuleType getType() const override{return ModuleType::Imager;}
    const std::vector<ParamDesc>& getParamDescs()const override{return descs;}
    void prepare(double rate,int,int)override
    {
        sr=rate;
        fastCoeff=msToCoeff(5,sr);slowCoeff=msToCoeff(60,sr);
        duckAttack=msToCoeff(8,sr);duckRelease=msToCoeff(40,sr);
        for(auto* s:{&lo,&hi,&trans,&mode})s->reset(sr,.03);
        lo.setCurrentAndTargetValue(getParam(0)/100);hi.setCurrentAndTargetValue(getParam(1)/100);
        trans.setCurrentAndTargetValue(getParam(3)/100);mode.setCurrentAndTargetValue(getParam(4));
        reset();
    }
    int latencySamples()const override{return delay;}
    void reset()override
    {
        for(auto& r:ring)r.fill(0);
        pos=0;env=slow=duck=0;energyL=energyR=cross=0;meter.store(1);
        crossover=getParam(2);makeKernel(crossover);oldLow=lowKernel;fade=1;
    }
    float getParam(int i)const override{return i>=0&&i<5?parameters[(size_t)i].load():0;}
    void setParam(int i,float v)override
    {
        if(i<0||i>=5||!std::isfinite(v))return;
        const auto& p=descs[(size_t)i];
        parameters[(size_t)i].store(clamp(p.integer?std::round(v):v,p.min,p.max));
    }
    int getNumMeters()const override{return 1;}
    float getMeter(int=0)const override{return meter.load();}
    void process(juce::AudioBuffer<float>& b)override
    {
        if(b.getNumChannels()==0)return;
        juce::ScopedNoDenormals noDenormals;
        lo.setTargetValue(getParam(0)/100);hi.setTargetValue(getParam(1)/100);
        trans.setTargetValue(getParam(3)/100);mode.setTargetValue(getParam(4));
        const float target=getParam(2);
        if(std::abs(target-crossover)>.01f)
        {
            for(int i=0;i<taps;++i)oldLow[(size_t)i]=lerp(oldLow[(size_t)i],lowKernel[(size_t)i],fade);
            makeKernel(target);crossover=target;fade=0;
        }
        const int channels=std::min(2,b.getNumChannels());
        for(int i=0;i<b.getNumSamples();++i)
        {
            const float l=b.getSample(0,i),r=b.getSample(channels-1,i),m=.5f*(l+r);
            ring[0][(size_t)pos]=l;ring[1][(size_t)pos]=r;
            float full=0,lowNow=0,lowBefore=0;
            // Paired taps exploit exact antisymmetry and halve the work.
            for(int k=0;k<delay;++k)
            {
                const int a=(pos-k+size)%size,bp=(pos-(taps-1-k)+size)%size;
                const float difference=.5f*(ring[0][(size_t)a]+ring[1][(size_t)a]-ring[0][(size_t)bp]-ring[1][(size_t)bp]);
                full+=difference*kernel[(size_t)k];lowNow+=difference*lowKernel[(size_t)k];
                if(fade<1)lowBefore+=difference*oldLow[(size_t)k];
            }
            const float lowBand=fade<1?lerp(lowBefore,lowNow,fade):lowNow;
            fade=std::min(1.f,fade+1.f/(float)(sr*.03));
            const int read=(pos-delay+size)%size;
            const float dryL=ring[0][(size_t)read],dryR=ring[1][(size_t)read];
            const float absMid=std::abs(.5f*(dryL+dryR));
            env+=fastCoeff*(absMid*absMid-env);slow+=slowCoeff*(absMid*absMid-slow);
            const float baseline=std::sqrt(std::max(0.f,slow));
            const float attack=clamp((std::sqrt(std::max(0.f,env))-baseline*1.3f)/(baseline+.02f),0,1);
            duck+=(attack>duck?duckAttack:duckRelease)*(attack-duck);
            const float lowWidth=lo.getNextValue(),highWidth=hi.getNextValue();
            const float strength=lerp(.55f,.95f,mode.getNextValue())*(1-trans.getNextValue()*duck);
            const float side=strength*(lowWidth*lowBand+highWidth*(full-lowBand));
            // Preserve the original center and side. Added side cancels exactly in mono.
            const float outL=dryL+side,outR=dryR-side;
            if(channels==1)b.setSample(0,i,dryL);else {b.setSample(0,i,outL);b.setSample(1,i,outR);}
            energyL+=.001f*(outL*outL-energyL);energyR+=.001f*(outR*outR-energyR);cross+=.001f*(outL*outR-cross);
            pos=(pos+1)%size;juce::ignoreUnused(m);
        }
        meter.store(clamp(cross/std::sqrt(energyL*energyR+1.e-20f),-1,1));
    }
private:
    static constexpr int taps=1025,delay=(taps-1)/2,size=2048;
    void makeKernel(float frequency)
    {
        for(int i=0;i<taps;++i)
        {
            const int n=i-delay;const float window=.42f-.5f*std::cos(kTwoPi*(float)i/(taps-1))+.08f*std::cos(2*kTwoPi*(float)i/(taps-1));
            kernel[(size_t)i]=n==0?0:window*(1-std::cos(kPi*(float)n))/(kPi*(float)n);
            lowKernel[(size_t)i]=n==0?0:window*(1-std::cos(kTwoPi*frequency*(float)n/(float)sr))/(kPi*(float)n);
        }
    }
    double sr=44100;int pos=0;float env=0,slow=0,duck=0,fastCoeff=0,slowCoeff=0,duckAttack=0,duckRelease=0,crossover=250,fade=1,energyL=0,energyR=0,cross=0;
    std::array<std::array<float,size>,2> ring{};
    std::array<float,taps> kernel{},lowKernel{},oldLow{};
    juce::SmoothedValue<float> lo,hi,trans,mode;
    std::array<std::atomic<float>,5> parameters{{20.f,50.f,250.f,80.f,0.f}};
    std::atomic<float> meter{1};
    inline static const std::vector<ParamDesc> descs{
        {"lo","Lo",0,100,20,""},{"hi","Hi",0,100,50,""},{"xover","Xover",100,4000,250," Hz"},
        {"trans","Trans",0,100,80,"%"},{"mode","Mode",0,1,0,"",true,{"Smooth","Wide"}}
    };
};
}
