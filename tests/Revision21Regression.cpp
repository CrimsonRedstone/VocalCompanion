#include "PluginProcessor.h"
#include "DSP/Drive.h"
#include "DSP/WaveShaper.h"
#include <iostream>
#include <stdexcept>
namespace
{
void check21(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
float tuneResponse(float speed,float flex)
{
    vc::AutoTuneModule tune;tune.setParam(0,speed);tune.setParam(1,flex);tune.setParam(2,0);tune.setParam(4,1);tune.setParam(5,0);tune.prepare(48000,256,2);
    juce::AudioBuffer<float> b(2,256);double phase=0;
    for(int block=0;block<240;++block)
    {
        const float hz=vc::midiToHz(block<200?69.f:69.4f);
        for(int i=0;i<256;++i){phase+=vc::kTwoPi*hz/48000.;for(int c=0;c<2;++c)b.setSample(c,i,.2f*(float)std::sin(phase));}
        tune.process(b);
    }
    float in[8]{},out[8]{};tune.copyPitchTrace(in,out,8);float sum=0;
    for(int i=0;i<8;++i){check21(in[i]>0,"Pitch detector failed voiced fixture");sum+=std::abs(in[i]-out[i]);}
    return sum/8;
}
}
void runRevision21Regression()
{
    VocalCompanionProcessor p;p.getChain().clear();p.getChain().add(vc::ModuleType::Gain);p.rebindHostParams();
    auto* first=p.getChain().get(0);const auto id=first->instanceId.toString();
    auto* lane=p.hostParamFor(first,0);check21(lane!=nullptr,"Missing host lane");const int laneIndex=lane->getParameterIndex();
    lane->setValue(.75f);check21(std::abs(first->getParam(0)-12)<.001,"Automation not applied");
    p.checkpoint();p.getChain().duplicate(0);p.rebindHostParams();auto* second=p.getChain().get(1);
    check21(second->instanceId.toString()!=id&&second->getParam(0)==12,"Duplicate identity/state incorrect");
    check21(p.hostParamFor(second,0)!=lane,"Duplicate inherited first card's automation slot");
    lane->setValue(.25f);check21(first->getParam(0)==-12&&second->getParam(0)==12,"Automation transferred to duplicate");
    p.checkpoint();p.getChain().move(0,2);p.rebindHostParams();
    check21(p.hostParamFor(p.getChain().get(1),0)==lane,"Automation changed on reorder");
    p.checkpoint();p.getChain().remove(1);p.getChain().add(vc::ModuleType::Chorus,0);p.rebindHostParams();
    auto* chorus=p.getChain().get(0);float chorusRate=chorus->getParam(0);lane->setValue(1);
    check21(chorus->getParam(0)==chorusRate,"Deleted card's lane controlled another module");
    check21(p.undoEdit(),"Undo unavailable");
    check21(p.getChain().size()==2&&p.getChain().get(1)->instanceId.toString()==id,"Undo did not restore deleted card/order");
    check21(p.hostParamFor(p.getChain().get(1),0)==lane,"Undo lost automation identity");
    lane->setValue(.5f);check21(p.getChain().get(1)->getParam(0)==0,"Restored automation lane inactive");
    check21(p.redoEdit()&&p.getChain().get(0)->getType()==vc::ModuleType::Chorus,"Redo failed");
    check21(p.undoEdit(),"Second undo failed");
    juce::MemoryBlock state;p.getStateInformation(state);VocalCompanionProcessor restored;restored.setStateInformation(state.getData(),(int)state.getSize());
    check21(restored.hostParamFor(restored.getChain().get(1),0)->getParameterIndex()==laneIndex,"Session changed automation slot");
    // Fresh preset instances never claim lanes belonging to an earlier rack.
    auto old=restored.hostParamFor(restored.getChain().get(1),0);restored.loadPresetTree(restored.saveStateTree());
    float before=restored.getChain().get(1)->getParam(0);old->setValue(1);check21(restored.getChain().get(1)->getParam(0)==before,"Preset stole an old automation lane");
    vc::Chain waveChain;waveChain.add(vc::ModuleType::WaveShaper);auto* wave=dynamic_cast<vc::WaveShaperModule*>(waveChain.get(0));
    wave->setPoints({{-1,-.2f,0,0},{0,.3f,.5f,1},{1,.8f,-.4f,0}});const float y=wave->evaluateShape(.4f);
    wave->selectSnapshot(1);check21(wave->getParam(5)==0,"Wave B did not retain original mode");wave->selectSnapshot(0);
    check21(std::abs(wave->evaluateShape(.4f)-y)<1.e-6f,"Wave A lost points");waveChain.duplicate(0);
    auto* clone=dynamic_cast<vc::WaveShaperModule*>(waveChain.get(1));check21(std::abs(clone->evaluateShape(.4f)-y)<1.e-6f,"Duplicate lost curve");
    clone->setPoints({{-1,-1,0,0},{1,1,0,0}});check21(std::abs(wave->evaluateShape(.4f)-y)<1.e-6f,"Duplicate shares curve state");
    for(double sr:{44100.,48000.,96000.})for(bool rat:{false,true})
    {
        vc::DriveModule drive(rat);drive.prepare(sr,512,2);drive.setParam(3,0);int latency=drive.latencySamples();
        juce::AudioBuffer<float> b(2,512);b.clear();b.setSample(0,0,1);drive.process(b);
        check21(latency>0&&b.getSample(0,latency)==1,"Drive dry path not latency aligned");
        drive.reset();drive.setParam(3,100);drive.setParam(0,48);
        for(int block=0;block<60;++block){for(int i=0;i<512;++i)for(int c=0;c<2;++c)b.setSample(c,i,.2f*std::sin((float)(vc::kTwoPi*220*(block*512+i)/sr)));drive.process(b);for(int c=0;c<2;++c)for(int i=0;i<512;++i)check21(std::isfinite(b.getSample(c,i))&&std::abs(b.getSample(c,i))<4,"Drive unstable");}
        drive.reset();b.clear();drive.process(b);check21(b.getMagnitude(0,512)<1.e-6f,"Drive reset produced noise");
    }
    vc::DynamicEqModule eq;eq.setParam(6,6);auto eqState=eq.toValueTree();vc::DynamicEqModule eq2;eq2.fromValueTree(eqState);check21(eq2.getParam(6)==6,"Dynamic EQ Q not persisted");
    const float fast=tuneResponse(0,0),slow=tuneResponse(400,0),flex=tuneResponse(0,100);
    std::cout<<"Retune correction: fast="<<fast<<" slow="<<slow<<" flex100="<<flex<<'\n';
    check21(fast>.25f&&fast>slow*1.5f&&flex<.02f,"Retune/Flex do not change correction meaningfully");
    std::cout<<"PASS: unique automation across duplicate/reorder/delete/undo/redo/session/preset, point-curve state, oversampled drive safety/latency, Dynamic EQ Q and Retune/Flex response\n";
}
