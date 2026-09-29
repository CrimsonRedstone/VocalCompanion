#include "PluginProcessor.h"
#include "DSP/Aeterna.h"
#include <iostream>
#include <stdexcept>
namespace
{
void check18(bool pass,const char* msg){if(!pass)throw std::runtime_error(msg);}
void finite18(const juce::AudioBuffer<float>& b){for(int c=0;c<b.getNumChannels();++c)for(int i=0;i<b.getNumSamples();++i)check18(std::isfinite(b.getSample(c,i))&&std::abs(b.getSample(c,i))<10,"Cleanup unstable");}
}
void runRevision18Regression()
{
    for(double sr:{44100.,48000.,96000.})
    {
        vc::BreathControlModule breath;breath.prepare(sr,256,2);breath.setParam(4,0);
        juce::AudioBuffer<float> impulse(2,16);impulse.clear();impulse.setSample(0,0,.1f);breath.process(impulse);
        check18(std::abs(impulse.getSample(0,2)-.1f)<1.e-7f,"Breath latency mismatch");
        breath.reset();breath.setParam(4,100);impulse.clear();impulse.setSample(0,5,.8f);breath.process(impulse);
        check18(impulse.getMagnitude(0,16)<1.e-6f,"Single-sample mouth click not repaired");
        breath.reset();breath.setParam(4,0);breath.setParam(1,80);breath.setParam(2,-12);
        juce::Random rng(183);double wet=0,dry=0;juce::AudioBuffer<float> b(2,256);
        for(int block=0;block<(int)(sr*1.5/256);++block)
        {
            for(int i=0;i<256;++i){float x=(rng.nextFloat()*2-1)*.04f;for(int c=0;c<2;++c)b.setSample(c,i,x);if(block>sr/256)dry+=x*x;}
            breath.process(b);finite18(b);if(block>sr/256)for(int i=0;i<256;++i)wet+=b.getSample(0,i)*b.getSample(0,i);
        }
        std::cout<<"Breath noise energy ratio "<<sr<<": "<<wet/dry<<'\n';
        check18(wet<dry*.65,"Breath attenuation ineffective on sustained unvoiced fixture");
        breath.reset();wet=dry=0;
        for(int block=0;block<(int)(sr/256);++block)
        {
            for(int i=0;i<256;++i){float x=.05f*std::sin(vc::kTwoPi*220*(block*256+i)/(float)sr);for(int c=0;c<2;++c)b.setSample(c,i,x);if(block>sr/512)dry+=x*x;}
            breath.process(b);if(block>sr/512)for(int i=0;i<256;++i)wet+=b.getSample(0,i)*b.getSample(0,i);
        }
        check18(std::abs(wet/dry-1)<.02,"Breath control attenuates sustained voiced fixture");
        vc::VocalRiderModule rider;rider.prepare(sr,256,2);rider.setParam(0,-18);rider.setParam(1,18);rider.setParam(3,100);
        double measured[2]{};
        for(int section=0;section<2;++section)for(int block=0;block<(int)(sr*2/256);++block)
        {
            for(int c=0;c<2;++c)for(int i=0;i<256;++i)b.setSample(c,i,(section?.5f:.05f)*std::sin(vc::kTwoPi*220*(block*256+i)/(float)sr));
            rider.process(b);finite18(b);if(block>sr/256)measured[section]+=b.getRMSLevel(0,0,256);
        }
        std::cout<<"Rider loud/quiet ratio "<<sr<<": "<<measured[1]/measured[0]<<'\n';
        check18(measured[1]/measured[0]<1.25&&measured[1]/measured[0]>.8,"Rider fails phrase-level consistency");
        for(int block=0;block<(int)(sr*2/256);++block){b.clear();rider.process(b);check18(b.getMagnitude(0,256)==0,"Rider creates audio from silence");}
        check18(std::abs(rider.getMeter())<.5,"Rider boosts silence indefinitely");
        vc::PlosiveControlModule plosive;plosive.prepare(sr,256,2);wet=dry=0;
        for(int block=0;block<(int)(sr*.3/256);++block)
        {
            for(int i=0;i<256;++i){float t=(block*256+i)/(float)sr;float x=t<.07f?.5f*std::sin(vc::kTwoPi*45*t):0;for(int c=0;c<2;++c)b.setSample(c,i,x);dry+=x*x;}
            plosive.process(b);finite18(b);for(int i=0;i<256;++i)wet+=b.getSample(0,i)*b.getSample(0,i);
        }
        std::cout<<"Plosive burst energy ratio "<<sr<<": "<<wet/dry<<'\n';
        check18(wet<dry*.85,"Plosive control did not reduce low burst");
        for(auto type:{vc::ModuleType::BreathControl,vc::ModuleType::VocalRider,vc::ModuleType::PlosiveControl})
        {
            auto m=vc::createModule(type);m->prepare(sr,256,1);m->setParam(0,m->getParamDescs()[0].max);
            for(int count:{1,63,257,4096}){juce::AudioBuffer<float> mono(1,count);mono.clear();m->process(mono);finite18(mono);}
            auto restored=vc::createModule(type);restored->fromValueTree(m->toValueTree());check18(std::abs(restored->getParam(0)-m->getParam(0))<1.e-6,"Cleanup state not preserved");
            m->reset();b.clear();m->process(b);check18(b.getMagnitude(0,256)==0,"Cleanup reset retained audio");
        }
    }
    juce::AudioBuffer<float> b(2,256);
    vc::Chain telemetry;telemetry.add(vc::ModuleType::PitchFormant);telemetry.prepare(48000,256,2);
    b.clear();telemetry.process(b,120);
    check18(telemetry.get(0)->cpuPercent.load()>0&&telemetry.get(0)->latencyMs.load()>0,"Card telemetry missing");
    // MIDI opt-in, channel filter, note-off, pedal and bypass delivery.
    vc::AeternaModule a;a.setParam(0,0);a.setParam(6,1);a.setParam(7,2);a.prepare(48000,256,2);
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);midi.addEvent(juce::MidiMessage::noteOn(2,64,.8f),100);b.clear();a.processWithMidi(b,midi,0);
    check18(a.getMeter(4)==1,"MIDI channel filter failed");
    a.handleMidi(juce::MidiMessage::controllerEvent(2,64,127));a.handleMidi(juce::MidiMessage::noteOff(2,64));check18(a.getMeter(4)==1,"Sustain pedal released note early");
    a.handleMidi(juce::MidiMessage::controllerEvent(2,64,0));check18(a.getMeter(4)==0,"Sustain release stuck note");
    a.handleMidi(juce::MidiMessage::noteOn(2,64,.8f));a.handleMidi(juce::MidiMessage::allNotesOff(2));check18(a.getMeter(4)==0,"All notes off failed");
    a.handleMidi(juce::MidiMessage::noteOn(2,64,.8f));a.clearMidi();check18(a.getMeter(4)==0,"Transport MIDI clear failed");
    auto saved=a.toValueTree();vc::AeternaModule restored;restored.fromValueTree(saved);check18(restored.getParam(6)==1&&restored.getParam(7)==2,"MIDI controls not saved");
    vc::Chain midiChain;midiChain.add(vc::ModuleType::Aeterna);midiChain.prepare(48000,256,2);auto* am=midiChain.get(0);am->setParam(0,0);am->setParam(6,1);
    midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,69,.8f),0);b.clear();midiChain.process(b,120,midi);check18(am->getMeter(4)==1,"MIDI not delivered through chain");
    am->setBypassed(true);midi.clear();midi.addEvent(juce::MidiMessage::noteOff(1,69),8);midiChain.process(b,120,midi);check18(am->getMeter(4)==0,"Bypassed Aeterna stuck MIDI note");
    // Optional MIDI does not alter the normal Scale mode, even with incoming notes.
    vc::AeternaModule dry,withMidi;dry.setParam(0,0);withMidi.setParam(0,0);dry.prepare(48000,256,2);withMidi.prepare(48000,256,2);
    midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,72,.8f),31);juce::AudioBuffer<float> other(2,256);
    for(int i=0;i<256;++i)for(int c=0;c<2;++c)b.setSample(c,i,.1f*std::sin(i*.1f));other.makeCopyOf(b);dry.process(b);withMidi.processWithMidi(other,midi,0);
    for(int i=0;i<256;++i)check18(b.getSample(0,i)==other.getSample(0,i),"MIDI changes default audio");
    // Confirm MIDI actually moves choir energy to the requested E4 (329.63 Hz).
    vc::AeternaModule sung;sung.setParam(0,127);sung.setParam(1,0);sung.setParam(2,0);sung.setParam(6,1);sung.prepare(48000,256,2);
    sung.handleMidi(juce::MidiMessage::noteOn(1,64,1.f));double real=0,imag=0;
    for(int block=0;block<500;++block)
    {
        for(int i=0;i<256;++i)for(int c=0;c<2;++c)b.setSample(c,i,.2f*std::sin(vc::kTwoPi*220*(block*256+i)/48000.f));
        const juce::MidiBuffer noEvents;sung.processWithMidi(b,noEvents,0);finite18(b);
        if(block>=300)for(int i=0;i<256;++i){const double phase=vc::kTwoPi*vc::midiToHz(64)*(block*256+i)/48000.;real+=b.getSample(0,i)*std::cos(phase);imag+=b.getSample(0,i)*std::sin(phase);}
    }
    const double midiAmplitude=2*std::hypot(real,imag)/(200*256);std::cout<<"MIDI choir E4 amplitude: "<<midiAmplitude<<'\n';
    check18(midiAmplitude>.003,"MIDI notes do not control the choir's audible pitch");
    // The wrapper is still an audio effect and legacy slots after Aeterna stay put.
    VocalCompanionProcessor proc;proc.getChain().clear();proc.getChain().add(vc::ModuleType::Aeterna);proc.getChain().add(vc::ModuleType::Gain);proc.prepareToPlay(48000,256);
    check18(proc.acceptsMidi()&&!proc.isMidiEffect()&&!proc.producesMidi(),"Incorrect wrapper MIDI role");
    check18(proc.hostParamFor(proc.getChain().get(1),0)==proc.getParameters()[6],"Aeterna extension moved existing automation slots");
    proc.rackComparison.mode=1;auto state=proc.saveStateTree();proc.loadPresetTree(state);check18(proc.rackComparison.mode.load()==0,"Preset did not initialise A/B");
    std::cout<<"PASS: cleanup detection/voice preservation/phrase leveling/click latency/state/reset; card telemetry; MIDI channel/pedal/note-off/bypass/state/isolation and legacy automation\n";
}
