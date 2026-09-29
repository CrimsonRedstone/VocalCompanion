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
        auto* p = new BindableParam();
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
        loadStateTree (juce::ValueTree::fromXml (*xml));
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
    auto preset = tree.createCopy();
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
    m.selectSnapshot(slot);notifySnapshotParams(&m);
}
void VocalCompanionProcessor::selectRackSnapshot(int slot)
{
    const juce::ScopedLock lock(chainLock);slot=juce::jlimit(0,1,slot);
    const int active=rackComparison.mode.load();if(slot==active)return;
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

void VocalCompanionProcessor::rebindHostParams()
{
    int n = 0;
    const juce::ScopedLock sl (chainLock);
    // Keep pre-1.0.18 automation indices when Aeterna precedes another card.
    for (int pass=0;pass<2;++pass)
    for (int i = 0; i < chain.size(); ++i)
    {
        auto* m = chain.get (i);
        const auto& d = m->getParamDescs();
        const bool aeterna=m->getType()==vc::ModuleType::Aeterna;
        const int first=pass==0?0:6,last=pass==0?(aeterna?6:(int)d.size()):(aeterna?(int)d.size():6);
        for (int p = first; p < last && n < kHostParamCount; ++p)
        {
            auto* hp = hostParams[n++];
            hp->target = m;
            hp->targetIndex = p;
            hp->minV = d[(size_t) p].min;
            hp->maxV = d[(size_t) p].max;
            hp->defV = d[(size_t) p].def;
            hp->label = m->getDisplayName() + " " + d[(size_t) p].label;
            const float den = m->getParam (p);
            hp->norm.store ((hp->maxV > hp->minV) ? (den - hp->minV) / (hp->maxV - hp->minV) : 0.0f);
        }
    }
    for (; n < kHostParamCount; ++n)
    {
        hostParams[n]->target = nullptr;
        hostParams[n]->targetIndex = -1;
        hostParams[n]->label = "Unused";
        hostParams[n]->norm.store (0.0f);
    }
    if (wrapperType != wrapperType_Undefined)
        updateHostDisplay();
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
