#include "SpectralPitch.h"
#include "../../ThirdParty/signalsmith-stretch.h"
namespace vc
{
namespace
{
constexpr int q = 64;
const std::vector<ParamDesc> formantDescs {
        { "key",     "Key",     0, 12, 0, "", true, juce::StringArray { "Auto", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } },
        { "scale",   "Scale",   0, 3, 0, "", true, juce::StringArray { "Major", "Minor", "Pent", "Chrom" } },
        { "retune",  "Retune",  0, 400, 40, " ms" },
        { "formant", "Formant", -12, 12, 0, "" }
    };
const std::vector<ParamDesc> tunerDescs {
        { "retune", "Retune",  0, 400, 20, " ms" },
        { "flex",   "Flex",    0, 100,  0, " c" },
        { "human",  "Human",   0, 100,  0, " %" },
        { "throat", "Throat", -12, 12,  0, " st", false },
        { "key",    "Key",     0, 12,   0, "", true, juce::StringArray {
            "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" } },
        { "scale",  "Scale",   0, 3,    1, "", true, juce::StringArray {
            "Chrom", "Major", "Minor", "Pent" } }
    };
float snap (float midi, int key, int scale)
{
    const int major[] {0,2,4,5,7,9,11}, minor[] {0,2,3,5,7,8,10}, pent[] {0,2,4,7,9};
    if (scale == 0) return std::round (midi);
    const int* notes = scale == 1 ? major : scale == 2 ? minor : pent;
    const int count = scale == 3 ? 5 : 7;
    float best = midi, distance = 100;
    const int oct = (int) std::floor ((midi - (float) key) / 12);
    for (int o = oct - 1; o <= oct + 1; ++o)
        for (int i = 0; i < count; ++i)
        {
            const float note = (float) (o * 12 + key + notes[i]);
            if (std::abs (note - midi) < distance) { best = note; distance = std::abs (note - midi); }
        }
    return best;
}
}
struct SpectralPitchModule::Engine
{
    signalsmith::stretch::SignalsmithStretch<float> shifter;
    juce::AudioBuffer<float> input {2,q}, output {2,q};
    std::array<float,1024> ring {}, frame {};
    std::array<float,512> correlation {};
    std::array<float,12> histogram {};
    Biquad low;
    std::vector<float> referenceEnergy;
    int energyPosition = 0, analysisChannel = 0;
    std::array<float,2> analysisPower {};
    float channelCoeff = 0;
    float inputEnergy = 0, outputEnergy = 0, levelGain = 1, energyCoeff = 0, gainCoeff = 0;
    double sr = 48000;
    int channels = 2, position = 0, written = 0, write = 0, decimate = 4, decimator = 0, hop = 0, latency = 0;
    int autoKey = 0, observe = 0, traceTick = 0, previousTonic = -1, previousScale = -1;
    float midi = 60, confidence = 0, correction = 0, formant = 0, note = 60, voicedAge = 0;
    bool hadNote = false;
    void prepare (double rate, int ch)
    {
        channelCoeff = msToCoeff (50,rate);
        energyCoeff = msToCoeff (45,rate); gainCoeff = msToCoeff (80,rate);
        sr = rate; channels = juce::jlimit (1,2,ch);
        shifter.presetDefault (channels, (float) sr, true);
        latency = q + shifter.inputLatency() + shifter.outputLatency();
        referenceEnergy.assign ((size_t) latency + 1, 0);
        decimate = std::max (1,(int) (sr/12000));
        low.setLowpass (2000,.707f,sr);
        reset();
    }
    void reset()
    {
        shifter.reset(); input.clear(); output.clear(); ring.fill (0); frame.fill (0); histogram.fill (0); low.reset();
        position = written = write = decimator = hop = observe = traceTick = autoKey = 0;
        std::fill (referenceEnergy.begin(), referenceEnergy.end(), 0);
        energyPosition = analysisChannel = 0; analysisPower.fill (0); inputEnergy = outputEnergy = 0; levelGain = 1;
        previousTonic = previousScale = -1;
        midi = note = 60; confidence = correction = formant = voicedAge = 0; hadNote = false;
    }
    void detect (float x)
    {
        const float filtered = low.process (x);
        if (++decimator < decimate) return;
        decimator = 0;
        ring[(size_t) write] = filtered; write = (write+1)%1024; written = std::min (1024,written+1);
        if (++hop < 64 || written < 512) return;
        hop = 0;
        for (int i=0;i<512;++i) frame[(size_t)i]=ring[(size_t)((write+512+i)%1024)];
        const float ds = (float) sr/(float)decimate;
        const int first = std::max (2,(int)(ds/1100)), last=std::min (510,(int)(ds/65));
        float best=0;
        for(int lag=first;lag<=last;++lag)
        {
            float dot=0,energy=0;
            for(int i=0;i<512-lag;++i)
            {
                const float a=frame[(size_t)i],b=frame[(size_t)(i+lag)];dot+=a*b;energy+=a*a+b*b;
            }
            const float value=energy>1.e-5f?2*dot/energy:0;correlation[(size_t)lag]=value;
            best=std::max(best,value);
        }
        confidence=0;
        for(int lag=first+1;lag<last;++lag)
        {
            const float a=correlation[(size_t)(lag-1)],b=correlation[(size_t)lag],c=correlation[(size_t)(lag+1)];
            if(b>.78f && b>=best*.94f && b>a && b>=c)
            {
                const float fraction=.5f*(a-c)/(a-2*b+c);
                midi=hzToMidi(ds/((float)lag+fraction)); confidence=b; break;
            }
        }
        for (auto& h:histogram) h*=.996f;
        if(confidence>.78f) histogram[(size_t)(((int)std::round(midi)%12+12)%12)]+=.02f;
    }
    void render (bool tuning, const std::array<float,6>& p, float advanced0, float advanced1)
    {
        const float speed=tuning?p[0]:p[2], flex=tuning?p[1]/100:0, human=tuning?p[2]/100:0;
        const float tract=p[3];
        const int key=(int)(tuning?p[4]:p[0]);
        const int scale=tuning?(int)p[5]:((int)p[1]==3?0:(int)p[1]+1);
        if (++observe >= (int)(sr*.5/q))
        {
            observe=0;
            float best=-1;int root=autoKey;
            for (int k=0;k<12;++k)
            {
                float score=0;
                for(int pc=0;pc<12;++pc)
                    score+=histogram[(size_t)pc]*(std::abs(snap((float)pc,k,scale)-(float)pc)<.01f?1.f:-1.f);
                if(k==autoKey)score+=.03f;
                if(score>best){best=score;root=k;}
            }
            autoKey=root;
        }
        const bool voiced=confidence>.78f;
        const int useScale=!tuning&&key==0?0:scale;
        const int tonic=key>0?key-1:autoKey;
        if (tonic != previousTonic || useScale != previousScale) hadNote = false;
        previousTonic = tonic; previousScale = useScale;
        const float candidate=snap(midi,tonic,useScale);
        if(voiced)
        {
            if(!hadNote||std::abs(candidate-midi)+(tuning?advanced1*.01f:.12f)<std::abs(note-midi))
            {note=candidate;voicedAge=0;hadNote=true;}
            voicedAge+=(float)q/(float)sr;
        }
        else { voicedAge=0;hadNote=false; }
        float target=voiced?note-midi:0;
        // Flex preserves a cents-wide dead zone; formant module's 400ms is correction off.
        if(std::abs(target)<=flex || (!tuning&&speed>=380)) target=0;
        else target=std::copysign(std::abs(target)-flex,target);
        target=clamp(target,-12,12)*(tuning?advanced0:advanced1);
        const float tau=voiced?std::max(.01f,speed*(1+3*human*std::min(1.f,voicedAge/.2f))):35;
        // Retune is a time constant in milliseconds, measured after detection.
        // At zero use the target directly instead of an artificial minimum glide.
        if(voiced && speed<=0.f)correction=target;
        else correction+=(1-std::exp(-(float)q/((float)sr*tau*.001f)))*(target-correction);
        formant+=(1-std::exp(-(float)q/((float)sr*.04f)))*(tract-formant);
        shifter.setTransposeSemitones(correction+(tuning?0:advanced0));
        shifter.setFormantSemitones(formant,true);
        shifter.setFormantBase(voiced ? midiToHz(midi)/(float)sr : 0.f);
        const float* ins[] {input.getReadPointer(0),input.getReadPointer(1)};
        float* outs[] {output.getWritePointer(0),output.getWritePointer(1)};
        shifter.process(ins,q,outs,q);
    }
};
SpectralPitchModule::SpectralPitchModule(bool tuner):tuning(tuner)
{
    const auto& desc=getCoreParamDescs();
    for(size_t i=0;i<parameters.size();++i)parameters[i].store(i<desc.size()?desc[i].def:0);
}
SpectralPitchModule::~SpectralPitchModule()=default;
const std::vector<ParamDesc>& SpectralPitchModule::getCoreParamDescs() const{return tuning?tunerDescs:formantDescs;}
float SpectralPitchModule::getCoreParam(int i) const{return i>=0&&i<(int)getCoreParamDescs().size()?parameters[(size_t)i].load():0;}
void SpectralPitchModule::setCoreParam(int i,float v)
{
    if(i<0||i>=(int)getCoreParamDescs().size()||!std::isfinite(v))return;
    const auto& p=getCoreParamDescs()[(size_t)i];parameters[(size_t)i].store(clamp(p.integer?std::round(v):v,p.min,p.max));
}
void SpectralPitchModule::prepare(double sr,int,int ch){engine=std::make_unique<Engine>();engine->prepare(sr,ch);reset();}
int SpectralPitchModule::latencySamples()const{return engine?engine->latency:0;}
void SpectralPitchModule::reset()
{
    if(engine)engine->reset();hz.store(0);traceWrite.store(0);
    for(auto& v:traceIn)v.store(0);for(auto& v:traceOut)v.store(0);
}
void SpectralPitchModule::process(juce::AudioBuffer<float>& buffer)
{
    if(!engine||buffer.getNumChannels()<1)return;
    juce::ScopedNoDenormals noDenormals;
    auto& e=*engine;std::array<float,6> p{};for(int i=0;i<(int)getCoreParamDescs().size();++i)p[(size_t)i]=getParam(i);
    const int channels=std::min(buffer.getNumChannels(),e.channels);
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        for(int c=0;c<e.channels;++c)e.input.setSample(c,e.position,buffer.getSample(std::min(c,channels-1),i));
        // Smoothed power and hysteresis make detection independent of host block boundaries.
        for (int c = 0; c < 2; ++c)
        {
            const float x = e.input.getSample (std::min(c,e.channels-1),e.position);
            e.analysisPower[(size_t)c] += e.channelCoeff * (x*x-e.analysisPower[(size_t)c]);
        }
        const int other = 1-e.analysisChannel;
        if (e.analysisPower[(size_t)other] > 4*e.analysisPower[(size_t)e.analysisChannel]+1.e-7f)
            e.analysisChannel = other;
        e.detect (e.input.getSample (std::min(e.analysisChannel,e.channels-1),e.position));
        float reference = 0, wetEnergy = 0;
        for (int c = 0; c < channels; ++c)
        {
            reference += std::pow (e.input.getSample (c,e.position), 2);
            wetEnergy += std::pow (e.output.getSample (c,e.position), 2);
        }
        e.referenceEnergy[(size_t) e.energyPosition] = reference / (float) channels;
        e.energyPosition = (e.energyPosition + 1) % (int) e.referenceEnergy.size();
        const float alignedEnergy = e.referenceEnergy[(size_t) e.energyPosition];
        e.inputEnergy += e.energyCoeff * (alignedEnergy - e.inputEnergy);
        e.outputEnergy += e.energyCoeff * (wetEnergy / (float) channels - e.outputEnergy);
        float desiredGain = 1;
        if ((std::abs (e.correction) > .001f || std::abs (e.formant) > .001f)
            && e.outputEnergy > 1.e-8f && e.inputEnergy > 1.e-8f)
            desiredGain = clamp (std::sqrt (e.inputEnergy / e.outputEnergy), .5f, 2.f);
        e.levelGain += e.gainCoeff * (desiredGain - e.levelGain);
        for(int c=0;c<channels;++c)buffer.setSample(c,i,e.output.getSample(c,e.position) * e.levelGain);
        if(++e.position==q)
        {
            e.render(tuning,p,advanced(0),advanced(1));e.position=0;
            if(++e.traceTick>=8)
            {
                e.traceTick=0;const int w=traceWrite.load();const bool voiced=e.confidence>.78f;
                traceIn[(size_t)w].store(voiced?e.midi:0);traceOut[(size_t)w].store(voiced?e.midi+e.correction:0);
                traceWrite.store((w+1)%96);
            }
        }
    }
    hz.store(e.confidence>.78f?midiToHz(e.midi):0);
}
int SpectralPitchModule::copyPitchTrace(float* in,float* out,int maxN)const
{
    const int count=std::min(maxN,96),w=traceWrite.load();
    for(int i=0;i<count;++i){const size_t pos=(size_t)((w+96-count+i)%96);in[i]=traceIn[pos].load();out[i]=traceOut[pos].load();}
    return count;
}
}
