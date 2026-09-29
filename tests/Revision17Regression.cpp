#include "Chain.h"
#include "DSP/Aeterna.h"
#include "Presets.h"
#include <iostream>
#include <stdexcept>
namespace
{
void expect(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
float limiterTail(float sustain,float release,float curve)
{
    vc::LimiterModule m;m.setParam(0,-12);m.setParam(1,release);m.setParam(3,3);m.setParam(4,sustain);m.setParam(5,curve);m.prepare(48000,128,2);
    juce::AudioBuffer<float> b(2,128);
    for(int block=0;block<100;++block)
    {
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)b.setSample(c,i,(block<70?.9f:.1f)*std::sin(vc::kTwoPi*220*(block*128+i)/48000.f));
        m.process(b);
    }
    return m.getMeter(0);
}
}
void runRevision17Regression()
{
    for(double sr:{44100.,48000.,96000.})for(float attack:{0.f,1.5f,12.f,30.f})
    {
        vc::LimiterModule limiter;
        limiter.setParam(0,-6);limiter.setParam(2,9);limiter.setParam(3,attack);limiter.setParam(4,120);limiter.prepare(sr,257,2);
        int t=0;
        for(int size:{1,63,257,4096,8192})
        {
            juce::AudioBuffer<float> b(2,size);
            for(int i=0;i<size;++i,++t){float x=t%997==0?2.f:.8f*std::sin(vc::kTwoPi*330*t/(float)sr);b.setSample(0,i,x);b.setSample(1,i,x*.4f);}
            limiter.process(b);
            for(int i=0;i<size;++i)
            {
                float l=b.getSample(0,i),r=b.getSample(1,i);
                expect(std::isfinite(l)&&std::abs(l)<=vc::dbToGain(-6)+1.e-6f,"Limiter exceeded ceiling");
                expect(std::abs(r-l*.4f)<1.e-6f,"Limiter changed stereo balance");
            }
        }
        limiter.setParam(2,0);limiter.prepare(sr,257,2);
        int latency=limiter.latencySamples();
        expect(latency==(int)std::round(sr*attack*.001),"Limiter latency not tied to Attack");
        juce::AudioBuffer<float> impulse(2,latency+32);impulse.clear();impulse.setSample(0,0,.1f);limiter.process(impulse);
        expect(std::abs(impulse.getSample(0,latency)-.1f)<1.e-6f,"Limiter impulse does not match reported latency");
        limiter.reset();impulse.clear();limiter.process(impulse);
        expect(impulse.getMagnitude(0,impulse.getNumSamples())<1.e-8f,"Limiter reset retained delayed audio");
    }
    const float quick=limiterTail(0,30,1),slow=limiterTail(0,400,1),sustained=limiterTail(350,30,1),curved=limiterTail(0,30,8);
    std::cout<<"Limiter tail reductions: fast="<<quick<<" slow="<<slow<<" sustain="<<sustained<<" curve="<<curved<<'\n';
    expect(slow>quick+.05f,"Limiter Release has no envelope effect");
    expect(sustained>quick+.05f,"Limiter Sustain has no envelope effect");
    expect(curved>quick+.01f,"Limiter Curve has no envelope effect");
    vc::LimiterModule sat,plain;
    sat.setParam(6,-12);sat.prepare(48000,128,2);plain.prepare(48000,128,2);
    juce::AudioBuffer<float> a(2,1024),b(2,1024);a.clear();
    for(int c=0;c<2;++c)for(int i=0;i<1024;++i)a.setSample(c,i,.95f*std::sin(vc::kTwoPi*i/53));b.makeCopyOf(a);
    sat.process(a);plain.process(b);
    expect(a.getRMSLevel(0,512,512)<b.getRMSLevel(0,512,512)*.95f,"Limiter Saturation has no effect");
    juce::ValueTree old("MODULE");old.setProperty("type","limiter",nullptr);old.setProperty("ceil",-6,nullptr);old.setProperty("makeup",3,nullptr);
    vc::LimiterModule migrated;migrated.fromValueTree(old);expect(migrated.getParam(0)==-3&&migrated.getParam(2)==3,"Legacy limiter level migration failed");
    vc::LimiterModule restored;restored.fromValueTree(migrated.toValueTree());expect(restored.getParam(0)==-3,"Limiter state migrated twice");
    vc::AeternaModule choir;choir.setParam(3,12);choir.setParam(4,3);choir.setParam(0,0);choir.prepare(48000,128,2);
    auto state=choir.toValueTree();vc::AeternaModule restoredChoir;restoredChoir.fromValueTree(state);
    expect(restoredChoir.getParam(3)==12&&restoredChoir.getParam(4)==3,"Aeterna Auto/Chromatic state lost");
    expect(vc::AeternaModule::harmonyInterval(61.2f,0,3,2)==4,"Chromatic choir snapped incoming note");
    expect(vc::AeternaModule::harmonyInterval(61.2f,0,3,4)==7,"Chromatic fifth incorrect");
    juce::AudioBuffer<float> voice(2,128);
    for(int block=0;block<150;++block)
    {
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)voice.setSample(c,i,.2f*std::sin(vc::kTwoPi*220*(block*128+i)/48000.f));
        choir.process(voice);
    }
    expect(choir.getMeter(3)==9,"Aeterna Auto did not resolve an isolated A");
    choir.setParam(0,70);
    for(int scale=0;scale<4;++scale)
    {
        choir.setParam(4,(float)scale);
        for(int block=0;block<50;++block)
        {
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)voice.setSample(c,i,.2f*std::sin(vc::kTwoPi*220*(block*128+i)/48000.f));
            choir.process(voice);
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)expect(std::isfinite(voice.getSample(c,i))&&std::abs(voice.getSample(c,i))<5,"Aeterna Auto scale unstable");
        }
    }
    const char* categories[]={"Clean","Bright","Ambient","Cute","Chaos","Warm","Lead","Backing","Rap","Pitch","Creative"};
    auto presets=vc::getRackPresetEntries();
    for(auto* category:categories)
    {
        int count=0;
        for(auto& preset:presets)if(preset.key.startsWith("studio:v17-")&&preset.tags.startsWith(juce::String(category)+" /"))++count;
        expect(count>=5,"Missing five new presets in a sound category");
    }
    for(auto& preset:presets)if(preset.source!=vc::RackPresetEntry::User)
        expect(!preset.name.containsIgnoreCase("Synth V")&&!preset.name.containsIgnoreCase("Vocaloid"),"Branded built-in preset name remains");
    std::cout<<"PASS: limiter ceiling/latency/envelopes/stereo/reset/migration, Aeterna Auto and Chromatic, category additions and generic names\n";
}
