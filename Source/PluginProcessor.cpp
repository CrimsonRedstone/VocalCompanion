#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

VocalCompanionProcessor::VocalCompanionProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default rack matches the aesthetic reference: gain → de-ess → FET →
    // chorus → phaser → tremolo → auto-pan → delay.
    chain.add (vc::ModuleType::Gain);
    chain.add (vc::ModuleType::DeEsser);
    chain.add (vc::ModuleType::FetComp);
    chain.add (vc::ModuleType::Chorus);
    chain.add (vc::ModuleType::Phaser);
    chain.add (vc::ModuleType::Tremolo);
    chain.add (vc::ModuleType::AutoPan);
    chain.add (vc::ModuleType::Delay);

    for (int i = 0; i < kHostParamCount; ++i)
    {
        auto* p = new BindableParam(*this);
        p->label = "P" + juce::String (i + 1);
        addParameter (p);
        hostParams[i] = p;
    }
    rebindHostParams();
    initialiseRackSnapshots();
}

VocalCompanionProcessor::~VocalCompanionProcessor() = default;

void VocalCompanionProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;
    const int chs = getTotalNumOutputChannels();

    const juce::ScopedLock sl (chainLock);
    chain.prepare (sr, samplesPerBlock, chs);
    rackComparison.prepare(sr,(int)(sr*10));
    wasPlaying=false;
    inSm.reset (sr, 0.02);
    outSm.reset (sr, 0.02);
    inSm.setCurrentAndTargetValue (vc::dbToGain (masterInDb.load()));
    outSm.setCurrentAndTargetValue (vc::dbToGain (masterOutDb.load()));
    setLatencySamples (chain.latencySamples());
    rackLatencyMs.store((float)(chain.latencySamples()*1000/sr));
    rebindHostParams();
    updateHostDisplay();
}

void VocalCompanionProcessor::releaseResources()
{
    const juce::ScopedLock sl (chainLock);
    chain.reset();
    rackComparison.reset();
    wasPlaying=false;
}

bool VocalCompanionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void VocalCompanionProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n=buffer.getNumSamples(),inCh=getTotalNumInputChannels(),outCh=getTotalNumOutputChannels();
    for(int c=inCh;c<outCh;++c)buffer.clear(c,0,n);
    if(outCh>=2&&inCh==1)buffer.copyFrom(1,0,buffer,0,0,n);
    inSm.setTargetValue(vc::dbToGain(masterInDb.load()));outSm.setTargetValue(vc::dbToGain(masterOutDb.load()));
    double bpm=120;bool playing=false,known=false;
    if(auto* ph=getPlayHead())if(auto pos=ph->getPosition()){if(auto b=pos->getBpm())bpm=*b;playing=pos->getIsPlaying();known=true;}
    float inPeak=0,outPeak=0;
    {
        const juce::ScopedLock sl(chainLock);
        if(known&&wasPlaying&&!playing)chain.clearMidi();
        if(known)wasPlaying=playing;
        const int latency=chain.latencySamples();if(latency!=getLatencySamples())setLatencySamples(latency);
        rackLatencyMs.store((float)(latency*1000/sampleRate));
        if(n==0)for(const auto event:midi)if(event.numBytes<=3)for(int i=0;i<chain.size();++i)chain.get(i)->handleMidi(event.getMessage());
        for(int start=0;start<n;start+=vc::Comparison::chunkSize)
        {
            const int count=std::min(vc::Comparison::chunkSize,n-start);
            float* ptr[2]{};for(int c=0;c<outCh;++c)ptr[c]=buffer.getWritePointer(c,start);
            juce::AudioBuffer<float> part(ptr,outCh,count);
            rackComparison.begin(part,latency);
            for(int i=0;i<count;++i)
            {
                const float gain=inSm.getNextValue();
                for(int c=0;c<outCh;++c){const float x=part.getSample(c,i)*gain;inPeak=std::max(inPeak,std::abs(x));part.setSample(c,i,x);}
            }
            chain.process(part,bpm,midi,start);
            for(int i=0;i<count;++i)
            {
                const float gain=outSm.getNextValue();for(int c=0;c<outCh;++c)part.setSample(c,i,part.getSample(c,i)*gain);
            }
            rackComparison.end(part);
            for(int c=0;c<outCh;++c)outPeak=std::max(outPeak,part.getMagnitude(c,0,count));
        }
    }
    midi.clear();
    inputPeak.store(std::max(inPeak,inputPeak.load()*.9f));outputPeak.store(std::max(outPeak,outputPeak.load()*.9f));
}

juce::AudioProcessorEditor* VocalCompanionProcessor::createEditor()
{
    return new VocalCompanionEditor (*this);
}

void VocalCompanionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = saveStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalCompanionProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {undoStates.clear();redoStates.clear();loadStateTree (juce::ValueTree::fromXml (*xml));}
}

juce::ValueTree VocalCompanionProcessor::saveStateTree() const
{
    juce::ValueTree t ("STATE");
    t.setProperty ("in", masterInDb.load(), nullptr);
    t.setProperty ("out", masterOutDb.load(), nullptr);
    t.setProperty ("skin", skinIndex, nullptr);
    t.setProperty ("fav", favorites.joinIntoString (","), nullptr);
    t.setProperty ("name", currentPresetName, nullptr);
    const juce::ScopedLock sl (chainLock);
    t.appendChild (chain.toValueTree(), nullptr);
    juce::ValueTree ab("RACK_AB");const int active=rackComparison.mode.load();
    ab.setProperty("selected",active,nullptr);ab.setProperty("match",rackComparison.matchEnabled.load(),nullptr);
    for(int i=0;i<2;++i)
    {
        juce::ValueTree slot(i==0?"A":"B");
        slot.appendChild(i==active||!rackSnapshots[(size_t)i].isValid()?captureRackSnapshot():rackSnapshots[(size_t)i].createCopy(),nullptr);
        ab.appendChild(slot,nullptr);
    }
    t.appendChild(ab,nullptr);
    juce::ValueTree mapping("AUTOMATION");
    for (int i=0; i<kHostParamCount; ++i)
        if (hostParams[i]->cardId.isNotEmpty())
        {
            juce::ValueTree slot("SLOT");
            slot.setProperty("index",i,nullptr);
            slot.setProperty("card",hostParams[i]->cardId,nullptr);
            slot.setProperty("parameter",hostParams[i]->parameterId,nullptr);
            mapping.appendChild(slot,nullptr);
        }
    t.appendChild(mapping,nullptr);
    return t;
}

void VocalCompanionProcessor::loadStateTree (const juce::ValueTree& t)
{
    if (! t.isValid())
        return;
    masterInDb.store ((float) t.getProperty ("in", 0.0f));
    masterOutDb.store ((float) t.getProperty ("out", 0.0f));
    skinIndex = (int) t.getProperty ("skin", 0);
    favorites = juce::StringArray::fromTokens (t.getProperty ("fav", "").toString(), ",", "");
    currentPresetName = t.getProperty ("name", "Custom").toString();
    if(currentPresetName == "Vocaloid Lead Polish")currentPresetName="Vocal Lead Polish";
    if(currentPresetName == "Synth V Natural")currentPresetName="Natural Voice";
    const juce::ScopedLock sl (chainLock);
    restoreAutomationMap(t.getChildWithName("AUTOMATION"), restoringHistory);
    if (auto ch = t.getChildWithName ("CHAIN"); ch.isValid())
        chain.fromValueTree (ch);
    initialiseRackSnapshots();
    const auto ab=t.getChildWithName("RACK_AB");
    if(ab.isValid())
    {
        for(int i=0;i<2;++i)
        {
            auto state=ab.getChildWithName(i==0?"A":"B").getChild(0);
            if(state.hasType("STATE")&&state.getChildWithName("CHAIN").isValid())rackSnapshots[(size_t)i]=state.createCopy();
        }
        rackComparison.mode=juce::jlimit(0,1,(int)ab.getProperty("selected",0));
        rackComparison.matchEnabled=(bool)ab.getProperty("match",true);
    }
    rackComparison.reset();
    rebindHostParams();
}

void VocalCompanionProcessor::loadPresetTree (const juce::ValueTree& tree)
{
    // A rack preset changes the sound; session state also restores preferences.
    checkpoint();
    auto preset = tree.createCopy();
    preset.removeChild(preset.getChildWithName("AUTOMATION"),nullptr);
    for (auto m : preset.getChildWithName("CHAIN"))
        m.setProperty("id",juce::Uuid().toString(),nullptr);
    preset.appendChild(saveStateTree().getChildWithName("AUTOMATION").createCopy(),nullptr);
    // A preset becomes the new starting point for both slots. A DAW session
    // restores both slots via loadStateTree instead.
    preset.removeChild(preset.getChildWithName("RACK_AB"),nullptr);
    for(auto m:preset.getChildWithName("CHAIN"))m.removeChild(m.getChildWithName("CARD_AB"),nullptr);
    preset.setProperty ("skin", skinIndex, nullptr);
    preset.setProperty ("fav", favorites.joinIntoString (","), nullptr);
    loadStateTree (preset);
}

juce::ValueTree VocalCompanionProcessor::captureRackSnapshot() const
{
    juce::ValueTree t("STATE");t.setProperty("in",masterInDb.load(),nullptr);t.setProperty("out",masterOutDb.load(),nullptr);
    t.setProperty("name",currentPresetName,nullptr);t.appendChild(chain.toValueTree(),nullptr);return t;
}
void VocalCompanionProcessor::initialiseRackSnapshots()
{
    const juce::ScopedLock lock(chainLock);
    rackSnapshots[0]=captureRackSnapshot();rackSnapshots[1]=rackSnapshots[0].createCopy();rackComparison.mode=0;
}
void VocalCompanionProcessor::notifySnapshotParams(vc::VcModule* only)
{
    rebindHostParams();
    for(auto* p:hostParams)if(p->target!=nullptr&&(only==nullptr||p->target==only)){p->beginChangeGesture();p->setValueNotifyingHost(p->getValue());p->endChangeGesture();}
    setLatencySamples(chain.latencySamples());rackLatencyMs=(float)(chain.latencySamples()*1000/sampleRate);
}
void VocalCompanionProcessor::selectModuleSnapshot(vc::VcModule& m,int slot)
{
    const juce::ScopedLock lock(chainLock);
    checkpoint();
    m.selectSnapshot(slot);notifySnapshotParams(&m);
}
void VocalCompanionProcessor::selectRackSnapshot(int slot)
{
    const juce::ScopedLock lock(chainLock);slot=juce::jlimit(0,1,slot);
    const int active=rackComparison.mode.load();if(slot==active)return;
    checkpoint();
    rackSnapshots[(size_t)active]=captureRackSnapshot();
    const auto& state=rackSnapshots[(size_t)slot];const auto saved=state.getChildWithName("CHAIN");
    bool same=chain.size()==saved.getNumChildren();
    for(int i=0;same&&i<chain.size();++i)same=chain.get(i)->instanceId.toString()==saved.getChild(i).getProperty("id").toString()&&vc::typeId(chain.get(i)->getType())==saved.getChild(i).getProperty("type").toString();
    if(same)for(int i=0;i<chain.size();++i){auto* m=chain.get(i);m->fromValueTree(saved.getChild(i));m->restoreSnapshots(saved.getChild(i));}
    else chain.fromValueTree(saved);
    masterInDb=(float)state.getProperty("in",0.f);masterOutDb=(float)state.getProperty("out",0.f);
    currentPresetName=state.getProperty("name",currentPresetName).toString();
    rackComparison.mode=slot;notifySnapshotParams();
}

void VocalCompanionProcessor::randomizeAllParams()
{
    juce::Random rng;
    const juce::ScopedLock sl (chainLock);
    for (int i = 0; i < chain.size(); ++i)
    {
        auto* m = chain.get (i);
        const auto& d = m->getParamDescs();
        for (int p = 0; p < (int) d.size(); ++p)
        {
            float v = d[(size_t) p].min + rng.nextFloat() * (d[(size_t) p].max - d[(size_t) p].min);
            if (d[(size_t) p].id == "gain" || d[(size_t) p].id == "input" || d[(size_t) p].id == "makeup")
                v = juce::jlimit (d[(size_t) p].min, juce::jmin (d[(size_t) p].max, 6.0f), v);
            m->setParam (p, d[(size_t) p].integer ? std::round (v) : v);
        }
    }
}

void VocalCompanionProcessor::randomizeEverything()
{
    checkpoint();
    juce::Random rng;
    {
        const juce::ScopedLock sl (chainLock);
        chain.clear();
        const int n = 4 + rng.nextInt (5);
        bool used[64] {};
        for (int i = 0; i < n; ++i)
        {
            int idx = rng.nextInt (vc::kPaletteCount);
            int guard = 0;
            while (used[idx] && guard++ < 20)
                idx = rng.nextInt (vc::kPaletteCount);
            used[idx] = true;
            chain.add (vc::kPalette[idx]);
        }
    }
    randomizeAllParams();
    {
        const juce::ScopedLock sl (chainLock);
        bool hot = false;
        for (int i = 0; i < chain.size(); ++i)
        {
            auto* m = chain.get (i);
            const auto& d = m->getParamDescs();
            for (int p = 0; p < (int) d.size(); ++p)
                if ((d[(size_t) p].id == "drive" && m->getParam (p) > 0.55f)
                    || (d[(size_t) p].id == "gain" && m->getParam (p) > 4.0f)
                    || (d[(size_t) p].id == "mix" && m->getParam (p) > 0.7f && m->getType() == vc::ModuleType::Bitcrush))
                    hot = true;
        }
        chain.add (vc::ModuleType::Limiter);
        if (auto* lim = chain.get (chain.size() - 1))
            lim->setParam (0, hot ? -1.2f : -0.4f);
    }
    currentPresetName = "Chaos";
    rebindHostParams();
}

void VocalCompanionProcessor::toggleFavorite (const juce::String& n)
{
    if (favorites.contains (n)) favorites.removeString (n);
    else favorites.add (n);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalCompanionProcessor();
}

// Host automation uses a fixed slot number, but the slot's destination never
// follows the rack index. Deleted cards leave tombstones so old automation cannot
// silently control a new effect. UUIDs and parameter IDs survive sessions/undo.
void VocalCompanionProcessor::rebindHostParams()
{
    const juce::ScopedLock sl(chainLock);
    for (auto* hp : hostParams) { hp->target=nullptr; hp->targetIndex=-1; }
    for (int pass=0;pass<3;++pass)
    for (int i=0;i<chain.size();++i)
    {
        auto* m=chain.get(i); const auto& d=m->getParamDescs();
        const auto id=m->instanceId.toString();
        // Appended controls must not shift legacy automation slots on migration.
        const int legacy=m->getType()==vc::ModuleType::Aeterna?6:
                         m->getType()==vc::ModuleType::DynamicEq?6:m->coreParamCount();
        const int first=pass==0?0:pass==1?legacy:m->coreParamCount();
        const int last=pass==0?legacy:pass==1?m->coreParamCount():(int)d.size();
        for(int p=first;p<last;++p)
        {
            BindableParam* hp=nullptr;
            for(auto* candidate:hostParams)
                if(candidate->cardId==id && candidate->parameterId==d[(size_t)p].id){hp=candidate;break;}
            if(hp==nullptr)for(auto* candidate:hostParams)
                if(candidate->cardId.isEmpty()){hp=candidate;hp->cardId=id;hp->parameterId=d[(size_t)p].id;break;}
            if(hp==nullptr)continue; // A full registry must never steal an existing slot.
            hp->target=m;hp->targetIndex=p;
            hp->minV=d[(size_t)p].min;hp->maxV=d[(size_t)p].max;hp->defV=d[(size_t)p].def;
            hp->label=m->getDisplayName()+" ["+id.substring(0,6)+"] "+d[(size_t)p].label;
            hp->norm=(m->getParam(p)-hp->minV)/(hp->maxV-hp->minV);
        }
    }
    if(wrapperType!=wrapperType_Undefined)updateHostDisplay();
}

void VocalCompanionProcessor::BindableParam::setValue(float v)
{
    if(!std::isfinite(v))return;
    const juce::ScopedLock lock(owner.chainLock);
    norm=juce::jlimit(0.f,1.f,v);
    // Resolve against the live chain under its lock. No freed module pointer is
    // dereferenced while a host automation callback races a UI deletion.
    for(int i=0;i<owner.chain.size();++i)
    {
        auto* m=owner.chain.get(i);
        if(m->instanceId.toString()!=cardId)continue;
        const auto& d=m->getParamDescs();
        for(int p=0;p<(int)d.size();++p)if(d[(size_t)p].id==parameterId)
        {m->setParam(p,d[(size_t)p].min+norm.load()*(d[(size_t)p].max-d[(size_t)p].min));return;}
    }
}

void VocalCompanionProcessor::restoreAutomationMap(const juce::ValueTree& map,bool merge)
{
    if(!merge)for(auto* hp:hostParams){hp->cardId.clear();hp->parameterId.clear();}
    for(auto slot:map)
    {
        const int i=(int)slot.getProperty("index",-1);
        if(i<0||i>=kHostParamCount)continue;
        auto* hp=hostParams[i];
        if(merge&&hp->cardId.isNotEmpty())continue;
        hp->cardId=slot.getProperty("card").toString();hp->parameterId=slot.getProperty("parameter").toString();
    }
}

void VocalCompanionProcessor::checkpoint()
{
    if(restoringHistory)return;
    auto state=saveStateTree();
    if(!undoStates.empty()&&undoStates.back().isEquivalentTo(state))return;
    undoStates.push_back(state);if(undoStates.size()>50)undoStates.erase(undoStates.begin());
    redoStates.clear();
}

bool VocalCompanionProcessor::undoEdit()
{
    if(undoStates.empty())return false;
    redoStates.push_back(saveStateTree());auto state=undoStates.back();undoStates.pop_back();
    restoringHistory=true;loadStateTree(state);restoringHistory=false;
    if(onHistoryRestored)onHistoryRestored();return true;
}
bool VocalCompanionProcessor::redoEdit()
{
    if(redoStates.empty())return false;
    undoStates.push_back(saveStateTree());auto state=redoStates.back();redoStates.pop_back();
    restoringHistory=true;loadStateTree(state);restoringHistory=false;
    if(onHistoryRestored)onHistoryRestored();return true;
}

juce::AudioProcessorParameter* VocalCompanionProcessor::hostParamFor (vc::VcModule* m, int paramIndex)
{
    for (int i = 0; i < kHostParamCount; ++i)
        if (hostParams[i]->target == m && hostParams[i]->targetIndex == paramIndex)
            return hostParams[i];
    return nullptr;
}

double VocalCompanionProcessor::getTailLengthSeconds() const
{
    const juce::ScopedLock lock (chainLock);
    double tail = 2;
    bool solo = false;
    for (int i = 0; i < chain.size(); ++i)
        solo |= chain.get (i)->isSoloed() && ! chain.get (i)->isBypassed();
    for (int i = 0; i < chain.size(); ++i)
    {
        const auto* m = chain.get (i);
        if (! m->isBypassed() && (! solo || m->isSoloed())) tail += m->tailLengthSeconds();
    }
    return tail;
}
