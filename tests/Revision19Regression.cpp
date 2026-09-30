#include "PluginProcessor.h"
#include "Presets.h"
#include <iostream>
#include <stdexcept>
namespace { void verify19(bool pass,const char* msg){if(!pass)throw std::runtime_error(msg);} }
void runRevision19Regression()
{
    VocalCompanionProcessor proc;proc.getChain().clear();proc.getChain().add(vc::ModuleType::Gain);proc.prepareToPlay(48000,256);
    auto* m=proc.getChain().get(0);const float initialMode=m->getParam(1),initialGain=m->getParam(0);
    m->setParam(1,2);m->setParam(0,9);proc.selectModuleSnapshot(*m,1);
    verify19(m->getParam(1)==initialMode&&m->getParam(0)==initialGain,"B did not start at module init");
    m->setParam(0,-6);m->setBypassed(true);proc.selectModuleSnapshot(*m,0);
    verify19(m->getParam(1)==2&&m->getParam(0)==9&&!m->isBypassed(),"A settings were overwritten by B");
    verify19(std::abs(proc.hostParamFor(m,0)->getValue()-(9.f+24)/48)<.0001f,"Host value did not follow snapshot");
    proc.selectModuleSnapshot(*m,1);verify19(m->getParam(0)==-6&&m->isBypassed(),"B edits or bypass lost");
    // Session round-trip preserves both slots, even edits made after last switch.
    m->setParam(0,-3);juce::MemoryBlock session;proc.getStateInformation(session);
    VocalCompanionProcessor reload;reload.setStateInformation(session.getData(),(int)session.getSize());auto* loaded=reload.getChain().get(0);
    verify19(loaded->comparison.mode.load()==1&&loaded->getParam(0)==-3,"Session lost active B edits");
    reload.selectModuleSnapshot(*loaded,0);verify19(loaded->getParam(0)==9&&loaded->getParam(1)==2,"Session lost inactive A");
    // Preset loads use the saved active sound as the starting point of BOTH slots.
    proc.loadPresetTree(proc.saveStateTree());m=proc.getChain().get(0);
    verify19(m->comparison.mode.load()==0&&m->getParam(0)==-3,"Preset starting settings incorrect");
    m->setParam(0,6);proc.selectModuleSnapshot(*m,1);verify19(m->getParam(0)==-3,"Preset B incorrectly inherited subsequent edits");
    // Every stock card's full parameter set is restored independently.
    for(auto type:vc::kPalette)
    {
        vc::Chain chain;chain.add(type);auto* card=chain.get(0);const auto& d=card->getParamDescs();
        for(int i=0;i<(int)d.size();++i)card->setParam(i,d[(size_t)i].max);
        card->selectSnapshot(1);for(int i=0;i<(int)d.size();++i)verify19(std::abs(card->getParam(i)-d[(size_t)i].def)<.001,"Stock B initial parameter incorrect");
        for(int i=0;i<(int)d.size();++i)card->setParam(i,d[(size_t)i].min);
        card->selectSnapshot(0);for(int i=0;i<(int)d.size();++i)verify19(std::abs(card->getParam(i)-d[(size_t)i].max)<.001,"Stock A parameter restoration failed");
    }
    vc::Chain choir;choir.add(vc::ModuleType::Aeterna);auto* a=choir.get(0);a->setParam(6,1);a->setParam(7,12);a->selectSnapshot(1);
    verify19(a->getParam(6)==0&&a->getParam(7)==0,"Aeterna B choices not initialised");a->selectSnapshot(0);
    verify19(a->getParam(6)==1&&a->getParam(7)==12,"Aeterna MIDI choices not restored");
    // Whole rack remembers topology, gains and the local card comparisons.
    proc.getChain().clear();proc.getChain().add(vc::ModuleType::Gain);proc.masterInDb=0;proc.masterOutDb=0;proc.initialiseRackSnapshots();
    proc.getChain().get(0)->setParam(0,4);proc.getChain().add(vc::ModuleType::Delay);proc.masterOutDb=-5;
    proc.selectRackSnapshot(1);verify19(proc.getChain().size()==1&&proc.getChain().get(0)->getParam(0)==0&&proc.masterOutDb.load()==0,"Rack B not initialised from baseline");
    proc.getChain().get(0)->setParam(0,-7);proc.masterInDb=3;proc.selectRackSnapshot(0);
    verify19(proc.getChain().size()==2&&proc.getChain().get(0)->getParam(0)==4&&proc.masterOutDb.load()==-5,"Rack A topology or gains lost");
    const auto full=proc.saveStateTree();reload.loadStateTree(full);reload.selectRackSnapshot(1);
    verify19(reload.getChain().size()==1&&reload.getChain().get(0)->getParam(0)==-7&&reload.masterInDb.load()==3,"Session lost inactive rack B");
    // Multiple round trips must not recursively nest snapshot state.
    for(int i=0;i<8;++i){auto state=reload.saveStateTree();reload.loadStateTree(state);reload.selectRackSnapshot(i%2);}
    verify19(reload.saveStateTree().toXmlString().length()<30000,"A/B persistence grows recursively");
    // Both branches remain processed. Optional Match corrects only B relative to A.
    vc::Comparison match;match.prepare(48000,1024);juce::AudioBuffer<float> buffer(2,256);int time=0;
    auto run=[&](int slot,float gain){match.mode=slot;for(int i=0;i<256;++i)for(int c=0;c<2;++c)buffer.setSample(c,i,.05f*std::sin(vc::kTwoPi*440*(time+i)/48000.f));time+=256;match.begin(buffer,0);buffer.applyGain(gain);match.end(buffer);};
    for(int i=0;i<600;++i)run(0,2);
    for(int i=0;i<600;++i)run(1,8);
    verify19(match.ready&&std::abs(match.matchDb.load()+12.0412f)<.05,"B not matched to A's processed level");
    verify19(std::abs(buffer.getRMSLevel(0,0,256)-.1f/std::sqrt(2.f))<.004,"Matched B audio incorrect");
    match.matchEnabled=false;for(int i=0;i<40;++i)run(1,8);
    verify19(buffer.getRMSLevel(0,0,256)>.25f,"Unmatched B reverted to dry or retained compensation");
    for(int i=0;i<40;++i)run(0,2);verify19(buffer.getRMSLevel(0,0,256)>.06f,"A reverted to dry");
    std::cout<<"PASS: independent per-card init/preset/session A/B, all stock parameters and MIDI choices, host values, rack topology/gains/session state, bounded serialization and processed A/B gain matching\n";
}
