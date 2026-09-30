#include "DSP/AutoTune.h"
#include "DSP/Pitch.h"
#include "DSP/StereoWidth.h"
#include "Chain.h"
#include <iostream>
#include <stdexcept>
namespace
{
void ensure(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
float amplitude(const std::vector<float>& signal,double sr,float hz)
{
    double real=0,imag=0;
    const int start=(int)signal.size()/2,n=(int)signal.size()-start;
    for(int i=0;i<n;++i)
    {
        const double window=.5-.5*std::cos(2*juce::MathConstants<double>::pi*i/(n-1));
        const double phase=2*juce::MathConstants<double>::pi*hz*i/sr;
        real+=signal[(size_t)(start+i)]*window*std::cos(phase);
        imag+=signal[(size_t)(start+i)]*window*std::sin(phase);
    }
    return (float)(4*std::hypot(real,imag)/n);
}
std::vector<float> render(vc::VcModule& module,double rate,int count,bool vowel=false)
{
    std::vector<float> result;result.reserve((size_t)count);
    int done=0;
    const int blocks[]={1,63,257,128,511};int bi=0;
    while(done<count)
    {
        int n=std::min(count-done,blocks[bi++%5]);juce::AudioBuffer<float>b(2,n);
        for(int i=0;i<n;++i)
        {
            float x=0;const float phase=vc::kTwoPi*(vowel?220.f:226.f)*(float)(done+i)/(float)rate;
            if(vowel)
                for(int h=1;h<=30;++h)
                {
                    const float f=220.f*(float)h;
                    const float envelope=.03f+std::exp(-std::pow((f-700)/180,2.f))+ .7f*std::exp(-std::pow((f-1220)/200,2.f))+.5f*std::exp(-std::pow((f-2600)/300,2.f));
                    x+=.04f*envelope*std::sin(phase*(float)h);
                }
            else x=.2f*std::sin(phase);
            b.setSample(0,i,x);b.setSample(1,i,x*.5f);
        }
        module.process(b);
        for(int i=0;i<n;++i)
        {
            const float l=b.getSample(0,i),r=b.getSample(1,i);
            ensure(std::isfinite(l)&&std::isfinite(r)&&std::abs(l)<2,"Pitch output unstable");
            ensure(std::abs(r-.5f*l)<.005f,"Spectral pitch lost stereo channel relationship");
            result.push_back(l);
        }
        done+=n;
    }
    return result;
}
}
void runAudioRepairRegression()
{
    for(double rate:{44100.,48000.,96000.})
    {
        vc::AutoTuneModule tuner;tuner.setParam(0,0);tuner.setParam(5,0);tuner.prepare(rate,257,2);
        auto corrected=render(tuner,rate,(int)rate);
        const float target=amplitude(corrected,rate,220),original=amplitude(corrected,rate,226);
        std::cout<<"Tuner "<<rate<<": 220 Hz="<<target<<", 226 Hz="<<original<<"\n";
        ensure(target>.16f&&target<.25f&&target>original*3,"Auto Tune failed to correct 226 Hz to 220 Hz");
        double quietest=1, loudest=0;
        const int window=(int)(rate*.05);
        for(int start=(int)rate/2;start+window<(int)corrected.size();start+=window)
        {
            double energy=0;
            for(int i=0;i<window;++i)energy+=std::pow(corrected[(size_t)(start+i)],2);
            quietest=std::min(quietest,energy/window);loudest=std::max(loudest,energy/window);
        }
        std::cout << "Steady level energy ratio=" << quietest/loudest << "\n";
        ensure(quietest/loudest>.8,"Pitch correction pumps or drops out on a steady voiced input");
        tuner.reset();tuner.setParam(1,100);
        auto flex=render(tuner,rate,(int)rate);
        ensure(amplitude(flex,rate,226)>.15f,"Flex failed to preserve input within dead zone");
        vc::PitchFormantModule neutral;neutral.setParam(2,400);neutral.prepare(rate,257,2);
        const int length=neutral.latencySamples()+8192;juce::AudioBuffer<float> impulse(2,length);impulse.clear();impulse.setSample(0,0,1);impulse.setSample(1,0,.5f);neutral.process(impulse);
        int peak=0;float maximum=0;
        for(int i=0;i<length;++i)if(std::abs(impulse.getSample(0,i))>maximum){maximum=std::abs(impulse.getSample(0,i));peak=i;}
        std::cout<<"Pitch latency "<<rate<<": reported="<<neutral.latencySamples()<<", impulse="<<peak<<", peak="<<maximum<<"\n";
        ensure(std::abs(peak-neutral.latencySamples())<=1&&maximum>.9f,"Spectral pitch latency or neutral gain incorrect");
    }
    {
        std::array<std::vector<float>,2> outputs;
        for(int pass=0;pass<2;++pass)
        {
            vc::AutoTuneModule tuner;tuner.setParam(0,0);tuner.setParam(5,0);tuner.prepare(48000,257,2);
            const int count=24000;int done=0;
            while(done<count)
            {
                const int n=std::min(count-done,pass==0?1:257);juce::AudioBuffer<float>b(2,n);
                for(int i=0;i<n;++i)
                {
                    const float phase=vc::kTwoPi*226.f*(float)(done+i)/48000.f;
                    b.setSample(0,i,.2f*std::sin(phase));b.setSample(1,i,.2f*std::cos(phase));
                }
                tuner.process(b);
                for(int i=0;i<n;++i)outputs[(size_t)pass].push_back(b.getSample(0,i));
                done+=n;
            }
            ensure(std::abs(tuner.getMeter()-226)<.2f,"Stereo phase offset corrupted pitch detection");
        }
        for(size_t i=0;i<outputs[0].size();++i)
            ensure(std::abs(outputs[0][i]-outputs[1][i])<1.e-5f,"Tuner depends on host buffer partitioning");
    }
    vc::PitchFormantModule plain,bright;plain.setParam(2,400);bright.setParam(2,400);bright.setParam(3,6);
    plain.prepare(48000,257,2);bright.prepare(48000,257,2);
    const auto low=render(plain,48000,48000,true),high=render(bright,48000,48000,true);
    double c0=0,c1=0,s0=0,s1=0;
    for(int h=1;h<31;++h)
    {
        const float f=220.f*(float)h,a=amplitude(low,48000,f),b=amplitude(high,48000,f);
        c0+=a*f;c1+=b*f;s0+=a;s1+=b;
    }
    std::cout<<"Vowel detected Hz="<<bright.getMeter()<<"\n";
    std::cout<<"Formant centroid: "<<c0/s0<<" -> "<<c1/s1<<" Hz\n";
    ensure(c1/s1>c0/s0*1.15,"Formant control failed to move spectral envelope");
    // Changing the envelope must not move the harmonic grid away from 220 Hz.
    ensure(amplitude(high,48000,880)>amplitude(high,48000,900)*4,"Formant shifting moved pitch");
    for (float extreme : {-12.f,12.f})
    {
        bright.setParam(3,extreme);
        auto extremeOutput=render(bright,48000,12000,true);
        ensure(!extremeOutput.empty(),"Missing extreme-formant output");
    }
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<2;++mode)for(float protection:{0.f,80.f,100.f})
    {
        vc::ImagerModule width;width.setParam(0,100);width.setParam(1,35);width.setParam(3,protection);width.setParam(4,(float)mode);width.prepare(rate,257,2);
        const int length=(int)rate+width.latencySamples();juce::AudioBuffer<float>b(2,length);
        std::vector<float> dry((size_t)length);
        for(int i=0;i<length;++i)
        {
            const float t=(float)i/(float)rate;
            const float envelope=.3f+.7f*std::pow(.5f+.5f*std::sin(vc::kTwoPi*3*t),4.f);
            const float x=.15f*envelope*(std::sin(vc::kTwoPi*110*t)+std::sin(vc::kTwoPi*997*t)+std::sin(vc::kTwoPi*4001*t));
            dry[(size_t)i]=x;b.setSample(0,i,x);b.setSample(1,i,x);
        }
        width.process(b);double left=0,right=0,side=0;float foldError=0;
        for(int i=2048;i<length;++i)
        {
            const float l=b.getSample(0,i),r=b.getSample(1,i);left+=l*l;right+=r*r;side+=(l-r)*(l-r);
            foldError=std::max(foldError,std::abs(.5f*(l+r)-dry[(size_t)(i-width.latencySamples())]));
        }
        const double balance=10*std::log10(left/right);
        std::cout<<"WIDTH "<<rate<<" mode "<<mode<<" transient "<<protection<<": balance="<<balance<<" dB, mono error="<<foldError<<"\n";
        ensure(std::abs(balance)<.1&&side>1,"WIDTH not balanced or not widening");
        ensure(foldError<1.e-6f,"WIDTH failed exact mono fold-down");
        width.setParam(0,0);width.setParam(1,0);width.prepare(rate,257,2);
        juce::AudioBuffer<float> impulse(2,2048);impulse.clear();impulse.setSample(0,0,.7f);impulse.setSample(1,0,-.2f);width.process(impulse);
        ensure(std::abs(impulse.getSample(0,width.latencySamples())-.7f)<1.e-6f&&std::abs(impulse.getSample(1,width.latencySamples())+.2f)<1.e-6f,"WIDTH zero setting changed stereo image");
    }
    vc::Chain chain;chain.add(vc::ModuleType::AutoTune);chain.add(vc::ModuleType::Imager);chain.prepare(48000,128,2);
    const int pitchLatency=chain.get(0)->latencySamples();ensure(chain.latencySamples()==pitchLatency+512,"Chain latency sum failed");
    chain.get(0)->setBypassed(true);ensure(chain.latencySamples()==512,"Bypassed latency counted");
    chain.get(0)->setBypassed(false);chain.get(0)->setSoloed(true);ensure(chain.latencySamples()==pitchLatency,"Non-solo latency counted");
    std::cout<<"PASS: rebuilt pitch/formant, stereo relationship, measured latency, WIDTH balance/mono and bypass/solo latency\n";
}
