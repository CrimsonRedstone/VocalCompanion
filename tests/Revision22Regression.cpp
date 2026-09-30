#include "PluginProcessor.h"
#include "UI/ModuleCard.h"
#include <iostream>
#include <stdexcept>

namespace {
void require22(bool value, const juce::String& message)
{
    if(!value) throw std::runtime_error(message.toStdString());
}
void configure(vc::VcModule& m)
{
    // Exercise non-unity processing so frequency/timing controls have a signal
    // to act on. These fixtures deliberately contain bursts, pauses and stereo.
    switch(m.getType())
    {
        case vc::ModuleType::DeEsser: m.setParam(0,-45);m.setParam(1,-45);break;
        case vc::ModuleType::Gain: m.setParam(1,2);m.setParam(2,.8f);break;
        case vc::ModuleType::DynamicEq: for(int i=0;i<4;++i)m.setParam(i,8);break;
        case vc::ModuleType::ParaEq: for(int i=0;i<3;++i)m.setParam(i,8);break;
        case vc::ModuleType::MsEq: for(int i=0;i<6;++i)m.setParam(i,8);break;
        case vc::ModuleType::BreathControl: m.setParam(1,100);m.setParam(2,-6);break;
        case vc::ModuleType::PlosiveControl: m.setParam(2,-45);m.setParam(3,2);break;
        case vc::ModuleType::Delay: m.setParam(0,40);m.setParam(3,0);m.setParam(m.coreParamCount(),.8f);break;
        case vc::ModuleType::Limiter: m.setParam(0,-15);break;
        case vc::ModuleType::Aeterna: m.setParam(0,100);m.setParam(1,75);m.setParam(2,75);break;
        default: break;
    }
}
}
void runRevision22Regression()
{
    std::vector<vc::ModuleType> types(std::begin(vc::kPalette),std::end(vc::kPalette));
    types.push_back(vc::ModuleType::Aeterna);
    VocalCompanionProcessor processor;
    for(auto type:types)
    {
        auto base=vc::createModule(type),changed=vc::createModule(type);
        configure(*base);configure(*changed);
        const int core=base->coreParamCount();const auto& desc=base->getParamDescs();
        require22((int)desc.size()>core,"No advanced controls for "+vc::typeName(type));
        for(int i=core;i<(int)desc.size();++i)
        {
            const auto& d=desc[(size_t)i];
            changed->setParam(i,d.min+(d.max-d.min)*.73f);
            require22(std::abs(changed->getParam(i)-(d.min+(d.max-d.min)*.73f))<.001f,"Advanced setter failed");
        }
        // Advanced values use exactly the same persistence path as core values.
        auto recalled=vc::createModule(type);recalled->fromValueTree(changed->toValueTree());
        for(int i=core;i<(int)desc.size();++i)require22(recalled->getParam(i)==changed->getParam(i),"Advanced recall failed");
        ModuleCard compact(*changed,processor,[]{},[]{}),full(*changed,processor,[]{},[]{},true);
        compact.setSize(ModuleCard::preferredWidth,compact.preferredHeight());full.setSize(680,full.preferredHeight());
        int compactCount=0,fullCount=0;
        for(auto* card:{&compact,&full})
        {
            int count=0;
            for(auto* child:card->getChildren())
            {
                if(dynamic_cast<ParamKnob*>(child)||dynamic_cast<ParamChoice*>(child))++count;
                if(child->isVisible())require22(card->getLocalBounds().contains(child->getBounds()),"Clipped full/compact control: "+vc::typeName(type));
            }
            if(card==&compact)compactCount=count;else fullCount=count;
        }
        require22(compactCount==core&&fullCount==(int)desc.size(),"Full/compact parameter split incorrect");
        for(auto* child:compact.getChildren())if(auto* button=dynamic_cast<juce::Button*>(child))
        {
            if(button->getButtonText()=="D"||button->getTitle()=="Full controls")
                require22(button->getY()<28&&button->getWidth()==button->getHeight(),"Card action is not square/in header");
        }
        base->prepare(48000,256,2);changed->prepare(48000,256,2);
        juce::AudioBuffer<float> a(2,256),b(2,256);juce::Random rng(1234);
        double difference=0,energy=0;
        for(int block=0;block<400;++block)
        {
            for(int i=0;i<256;++i)
            {
                const int n=block*256+i;const float t=n/48000.f;
                const float gate=(n%12000)<6000?1.f:.02f;
                const float breath=(n%24000)>16000?.10f*(rng.nextFloat()*2-1):0;
                for(int c=0;c<2;++c)
                {
                    const float x=gate*(.22f*std::sin(vc::kTwoPi*226*t+c*.4f)+.1f*std::sin(vc::kTwoPi*80*t)+.04f*std::sin(vc::kTwoPi*4500*t+c*.7f))+breath;
                    a.setSample(c,i,x);b.setSample(c,i,x);
                }
            }
            base->process(a);changed->process(b);
            for(int c=0;c<2;++c)for(int i=0;i<256;++i)
            {
                const float x=a.getSample(c,i),y=b.getSample(c,i);
                require22(std::isfinite(y)&&std::abs(y)<100,"Unstable advanced processing: "+vc::typeName(type));
                difference+=(x-y)*(x-y);energy+=x*x;
            }
        }
        std::cout<<"Advanced DSP "<<vc::typeId(type)<<": relative difference="<<difference/(energy+1.e-12)<<'\n';
        require22(difference>1.e-8,"Advanced controls have no audible-path effect: "+vc::typeName(type));
    }
    // Host lanes for appended parameters must survive duplication and history.
    processor.getChain().clear();processor.getChain().add(vc::ModuleType::Gain);processor.rebindHostParams();
    auto* module=processor.getChain().get(0);auto* lane=processor.hostParamFor(module,module->coreParamCount());
    require22(lane!=nullptr,"Missing advanced host parameter");lane->setValue(.2f);
    processor.checkpoint();processor.getChain().duplicate(0);processor.rebindHostParams();lane->setValue(.8f);
    require22(processor.getChain().get(0)->advanced(0)!=processor.getChain().get(1)->advanced(0),"Advanced duplicate shares automation");
    require22(processor.undoEdit()&&processor.getChain().size()==1,"Advanced history failed");
    std::cout<<"PASS: all 28 stock modules expose additional working DSP controls, state recall, compact/full layout, square header actions and independent advanced automation\n";
}
