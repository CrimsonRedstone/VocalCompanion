#include "Chain.h"
#include "DSP/Aeterna.h"
#include "Host/ExternalHost.h"

namespace vc
{

std::unique_ptr<VcModule> createModule (ModuleType type)
{
    switch (type)
    {
        case ModuleType::BreathControl: return std::make_unique<BreathControlModule>();
        case ModuleType::VocalRider: return std::make_unique<VocalRiderModule>();
        case ModuleType::PlosiveControl: return std::make_unique<PlosiveControlModule>();
        case ModuleType::Gain:         return std::make_unique<GainModule>();
        case ModuleType::DeEsser:      return std::make_unique<DeEsserModule>();
        case ModuleType::FetComp:      return std::make_unique<FetCompModule>();
        case ModuleType::OptoComp:     return std::make_unique<OptoCompModule>();
        case ModuleType::Limiter:      return std::make_unique<LimiterModule>();
        case ModuleType::DynamicEq:    return std::make_unique<DynamicEqModule>();
        case ModuleType::ParaEq:       return std::make_unique<ParaEqModule>();
        case ModuleType::PitchFormant: return std::make_unique<PitchFormantModule>();
        case ModuleType::AutoTune:     return std::make_unique<AutoTuneModule>();
        case ModuleType::Imager:       return std::make_unique<ImagerModule>();
        case ModuleType::MsEq:         return std::make_unique<MsEqModule>();
        case ModuleType::AirBreath:    return std::make_unique<AirBreathModule>();
        case ModuleType::Exciter:      return std::make_unique<ExciterModule>();
        case ModuleType::RingMod:      return std::make_unique<RingModModule>();
        case ModuleType::Bitcrush:     return std::make_unique<BitcrushModule>();
        case ModuleType::Chorus:       return std::make_unique<ChorusModule>();
        case ModuleType::Phaser:       return std::make_unique<PhaserModule>();
        case ModuleType::Tremolo:      return std::make_unique<TremoloModule>();
        case ModuleType::AutoPan:      return std::make_unique<AutoPanModule>();
        case ModuleType::Delay:        return std::make_unique<DelayModule>();
        case ModuleType::Reverb:       return std::make_unique<ReverbModule>();
        case ModuleType::Aeterna:      return std::make_unique<AeternaModule>();
        case ModuleType::External:     return std::make_unique<ExternalModule>();
    }
    return std::make_unique<GainModule>();
}

void Chain::prepare (double sr, int block, int chs)
{
    sampleRate = sr;
    blockSize = block;
    numCh = chs;
    prepared = true;
    for (auto& s : slots)
    {
        s->prepare (sr, block, chs);
        s->visual->prepare (sr);
        s->comparison.prepare(sr,(int)(sr*2));
        s->wasProcessing=false;
    }
}

void Chain::reset()
{
    for (auto& s : slots)
    { s->reset(); s->comparison.reset(); s->clearMidi(); s->wasProcessing=false; s->cpuPercent=0; }
}

void Chain::clearMidi() { for(auto& s:slots)s->clearMidi(); }
void Chain::process(juce::AudioBuffer<float>& buffer,double bpm)
{
    const juce::MidiBuffer empty;
    process(buffer,bpm,empty);
}
void Chain::process(juce::AudioBuffer<float>& buffer,double bpm,const juce::MidiBuffer& midi,int offset)
{
    bool anySolo=false;
    for(auto& s:slots)anySolo|=s->isSoloed()&&!s->isBypassed();
    for(auto& s:slots)
    {
        s->currentBpm=bpm;
        s->latencyMs.store((float)(s->latencySamples()*1000/sampleRate));
        if(s->isBypassed()||(anySolo&&!s->isSoloed()))
        {
            for(const auto event:midi)if(event.numBytes<=3&&event.samplePosition>=offset&&event.samplePosition<offset+buffer.getNumSamples())s->handleMidi(event.getMessage());
            s->cpuPercent=0;s->wasProcessing=false;continue;
        }
        if(!s->wasProcessing){s->comparison.reset();s->wasProcessing=true;}
        const auto started=juce::Time::getHighResolutionTicks();
        for(int pos=0;pos<buffer.getNumSamples();pos+=Comparison::chunkSize)
        {
            const int n=std::min(Comparison::chunkSize,buffer.getNumSamples()-pos);
            float* ptr[2]{};for(int c=0;c<std::min(2,buffer.getNumChannels());++c)ptr[c]=buffer.getWritePointer(c,pos);
            juce::AudioBuffer<float> part(ptr,std::min(2,buffer.getNumChannels()),n);
            s->comparison.begin(part,s->latencySamples());
            s->visual->bpm.store((float)bpm,std::memory_order_relaxed);
            s->visual->begin(part);
            s->processWithMidi(part,midi,offset+pos);
            s->visual->end(part);
            s->comparison.end(part);
        }
        if(buffer.getNumSamples()>0)
        {
            const float load=(float)(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-started)*sampleRate/buffer.getNumSamples()*100);
            const float smooth=(float)(1-std::exp(-buffer.getNumSamples()/(sampleRate*.5)));
            s->cpuPercent.store(s->cpuPercent.load()+smooth*(load-s->cpuPercent.load()));
        }
    }
}

int Chain::add (ModuleType type, int insertAt)
{
    auto m = createModule (type);
    m->initialiseSnapshots();
    if (prepared)
    {
        m->prepare (sampleRate, blockSize, numCh);
        m->visual->prepare (sampleRate);
        m->comparison.prepare(sampleRate,(int)(sampleRate*2));
    }
    const int idx = insertAt < 0 || insertAt > size() ? size() : insertAt;
    slots.insert (slots.begin() + idx, std::move (m));
    return idx;
}

void Chain::replace (int index, ModuleType type)
{
    if (index < 0 || index >= size()) return;
    auto m = createModule (type);
    m->initialiseSnapshots();
    if (prepared)
    {
        m->prepare (sampleRate, blockSize, numCh);
        m->visual->prepare (sampleRate);
        m->comparison.prepare(sampleRate,(int)(sampleRate*2));
    }
    slots[(size_t) index] = std::move (m);
}

void Chain::remove (int index)
{
    if (index >= 0 && index < size())
        slots.erase (slots.begin() + index);
}

void Chain::move (int from, int to)
{
    if (from < 0 || from >= size() || to < 0 || to > size() || from == to)
        return;
    auto m = std::move (slots[(size_t) from]);
    slots.erase (slots.begin() + from);
    if (to > from) --to;
    slots.insert (slots.begin() + to, std::move (m));
}

void Chain::clear()
{
    slots.clear();
}

juce::ValueTree Chain::toValueTree() const
{
    juce::ValueTree t ("CHAIN");
    for (auto& s : slots)
        t.appendChild (s->stateWithSnapshots(), nullptr);
    return t;
}

void Chain::fromValueTree (const juce::ValueTree& t)
{
    slots.clear();
    for (const auto& child : t)
    {
        auto type = typeFromId (child.getProperty ("type").toString());
        auto m = createModule (type);
    m->initialiseSnapshots();
        m->fromValueTree (child);
        m->restoreSnapshots(child);
        if (prepared)
        {
            m->prepare (sampleRate, blockSize, numCh);
            m->visual->prepare (sampleRate);
        m->comparison.prepare(sampleRate,(int)(sampleRate*2));
        }
        slots.push_back (std::move (m));
    }
}

int Chain::latencySamples() const
{
    int lat = 0;
    bool solo = false;
    for (const auto& s : slots) solo |= s->isSoloed() && !s->isBypassed();
    for (const auto& s : slots)
        if (!s->isBypassed() && (!solo || s->isSoloed())) lat += s->latencySamples();
    return lat;
}

} // namespace vc
