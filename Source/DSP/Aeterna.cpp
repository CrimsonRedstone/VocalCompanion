#include "Aeterna.h"
#include "../../ThirdParty/signalsmith-stretch.h"

namespace vc
{
namespace
{
constexpr int quantum = 64;
using Shifter = signalsmith::stretch::SignalsmithStretch<float>;
const std::vector<ParamDesc> descriptions {
    { "devotion", "Devotion", 0, 127, 40, "", true },
    { "glass", "Resonant Glass", 0, 100, 35, "%" },
    { "halo", "Halo Shimmer", 0, 100, 40, "%" },
    { "key", "Key", 0, 12, 0, "", true, { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "Auto" } },
    { "scale", "Scale", 0, 3, 0, "", true, { "Major", "Minor", "Pentatonic", "Chromatic" } },
    { "hold", "Sustain", 0, 1, 0, "", true, { "Flow", "Hold" } },
    { "harmonyMode", "Harmony", 0, 1, 0, "", true, { "Scale", "MIDI notes" } },
    { "midiChannel", "MIDI channel", 0, 16, 0, "", true, { "Any", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" } }
};
struct Delay
{
    std::vector<float> data;
    int pos = 0;
    void prepare (int size) { data.assign ((size_t) size, 0); pos = 0; }
    void reset() { std::fill (data.begin(), data.end(), 0); pos = 0; }
    float read (float delay) const
    {
        delay = juce::jlimit(1.f,(float)data.size()-2,delay);
        float p = (float) pos - delay;
        if (p < 0) p += (float) data.size();
        const int i = (int) p;
        return lerp (data[(size_t) i], data[(size_t) ((i + 1) % (int) data.size())], p - (float) i);
    }
    float head() const { return data[(size_t) pos]; }
    void push (float x) { data[(size_t) pos] = x; pos = (pos + 1) % (int) data.size(); }
};
// Original synthetic cathedral IR: diffuse exponentially decaying field with
// discrete early reflections. Generated only in prepare, never on the audio path.
juce::AudioBuffer<float> cathedralIR()
{
    constexpr int rate = 24000, length = rate * 6;
    juce::AudioBuffer<float> ir (2, length);
    ir.clear();
    juce::Random random (0x41455445524e41LL);
    for (int c = 0; c < 2; ++c)
    {
        float low = 0;
        for (int i = 900; i < length; ++i)
        {
            const float t = (float) i / rate;
            low += 0.32f * (random.nextFloat() * 2 - 1 - low);
            const float attack = std::min (1.0f, (float) (i - 900) / 2400.0f);
            ir.setSample (c, i, low * attack * std::exp (-1.65f * t) * 0.045f);
        }
        const int times[] { 733, 1061, 1453, 2081, 2803, 3671 };
        for (int i = 0; i < 6; ++i)
            ir.addSample (c, times[i] + c * (47 + 13 * i), 0.18f * std::exp (-0.35f * (float) i));
    }
    return ir;
}
}

float AeternaModule::harmonyInterval (float midi, int key, int scale, int degree)
{
    // Chromatic harmony follows every sung pitch, without snapping to a key.
    if (scale == 3) return degree == 2 ? 4.f : degree == 4 ? 7.f : (float)degree;
    key = juce::jlimit(0,11,key);
    static constexpr int scales[3][7] { {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10}, {0,2,4,7,9,0,0} };
    scale = juce::jlimit (0, 2, scale);
    const int count = scale == 2 ? 5 : 7;
    int nearest = 0;
    float distance = 1000;
    for (int i = -35; i < 70; ++i)
    {
        const int octave = (int) std::floor ((float) i / (float) count);
        const int note = key + octave * 12 + scales[scale][i - octave * count];
        if (std::abs ((float) note - midi) < distance) { distance = std::abs ((float) note - midi); nearest = i; }
    }
    const int target = nearest + degree;
    const int oct = (int) std::floor ((float) target / (float) count);
    return (float) (key + oct * 12 + scales[scale][target - oct * count]) - midi;
}

struct AeternaModule::Engine
{
    double sr = 48000;
    std::array<Shifter, 8> choir;
    std::array<Shifter, 3> shimmer;
    std::array<bool, 8> active {};
    std::array<float, 8> voiceGains {};
    juce::dsp::Convolution room { juce::dsp::Convolution::NonUniform { 512 } };
    juce::AudioBuffer<float> roomBuffer { 2, quantum };
    std::array<float, quantum> input {}, shifted {}, feedback {}, halo {}, source {};
    std::array<std::array<float, quantum>, 2> output {}, voices {};
    std::array<Delay, 6> glass;
    std::array<float, 6> glassDelay {};
    std::array<Delay, 8> sustain;
    std::array<float, 8> damping {};
    Biquad preHigh, preAir, feedbackHigh, feedbackLow, detectorLow;
    juce::SmoothedValue<float> devotion, glassAmount, haloAmount, hold;
    std::array<float, 512> history {}, analysis {};
    int pos = 0, detectorPos = 0, detectorCount = 0, decimation = 4, decimator = 0, pitchHop = 0;
    float midi = 60, confidence = 0, env = 0;
    std::array<float,12> keyEvidence {};
    int autoKey = 0, proposedKey = 0, keyVotes = 0, analysisScale = 0;
    bool keyEstablished = false;
    std::array<std::array<float,128>,16> midiVelocity {};
    std::array<std::array<bool,128>,16> keyDown {};
    std::array<bool,16> pedal {};
    std::array<float,8> midiShifts {};
    int midiMode=0, midiChannel=0;
    void clearNotes() { for(auto& a:midiVelocity)a.fill(0);for(auto& a:keyDown)a.fill(false);pedal.fill(false); }

    void updateKey()
    {
        if (confidence < .88f) return;
        for (auto& weight : keyEvidence) weight *= .995f;
        keyEvidence[(size_t)(((int)std::round(midi)%12+12)%12)] += 1;
        const int major[] {0,2,4,5,7,9,11}, minor[] {0,2,3,5,7,8,10}, pent[] {0,2,4,7,9};
        const auto* degrees = analysisScale == 1 ? minor : analysisScale == 2 ? pent : major;
        const int count = analysisScale == 2 ? 5 : 7;
        float best = -1.e9f, current = -1.e9f;
        int candidate = autoKey;
        for(int root=0;root<12;++root)
        {
            float score = 0;
            for(int pc=0;pc<12;++pc)
            {
                float weight=-.8f;
                for(int d=0;d<count;++d)if((root+degrees[d])%12==pc)weight=d==0?1.4f:degrees[d]==7?1.2f:1.f;
                score += keyEvidence[(size_t)pc]*weight;
            }
            if(root==autoKey)current=score;
            if(score>best){best=score;candidate=root;}
        }
        if(!keyEstablished){autoKey=candidate;keyEstablished=true;}
        else if(candidate!=autoKey && best>current*1.08f+1.f)
        {
            if(candidate==proposedKey)++keyVotes;else{proposedKey=candidate;keyVotes=1;}
            if(keyVotes>=24){autoKey=candidate;keyVotes=0;}
        }
        else keyVotes=0;
    }

    void prepare (double rate, float initialDevotion)
    {
        sr = rate;
        const int window = (int) std::round (sr * 0.064 / 64) * 64;
        for (auto& voice : choir) voice.configure (1, window, window / 4, true);
        for (int i = 0; i < 3; ++i)
        {
            shimmer[(size_t) i].configure (1, window, window / 4, true);
            shimmer[(size_t) i].setTransposeSemitones ((float) (i == 0 ? 7 : i == 1 ? 12 : 19));
        }
        room.loadImpulseResponse (cathedralIR(), 24000, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
        room.prepare ({ sr, quantum, 2 });
        for (auto& d : glass) d.prepare ((int) (sr / 80) + 4);
        const float delays[] { .0713f,.0839f,.0977f,.1091f,.1277f,.1399f,.1511f,.1637f };
        for (int i = 0; i < 8; ++i) sustain[(size_t) i].prepare ((int) (sr * delays[i]));
        preHigh.setHighpass (65, .707f, sr);
        preAir.setHighShelf (5500, 3.5f, sr);
        feedbackHigh.setHighpass (180, .707f, sr);
        feedbackLow.setLowpass (std::min (7000.0f, (float) sr * .4f), .707f, sr);
        decimation = std::max (1, (int) (sr / 12000));
        detectorLow.setLowpass (2000, .707f, sr);
        for (auto* sm : { &devotion, &glassAmount, &haloAmount, &hold }) sm->reset (sr, .08);
        reset();
        devotion.setCurrentAndTargetValue (initialDevotion);
    }
    void reset()
    {
        for (auto& s : choir) s.reset();
        for (auto& s : shimmer) s.reset();
        active.fill (false); voiceGains.fill (0);
        room.reset(); roomBuffer.clear();
        for (auto& d : glass) d.reset();
        for (auto& d : sustain) d.reset();
        damping.fill (0); glassDelay.fill (0);
        input.fill (0); feedback.fill (0); halo.fill (0); source.fill (0);
        for (auto& v : output) v.fill (0);
        history.fill (0); analysis.fill (0);
        preHigh.reset(); preAir.reset(); feedbackHigh.reset(); feedbackLow.reset(); detectorLow.reset();
        pos = detectorPos = detectorCount = decimator = pitchHop = 0;
        midi = 60; confidence = env = 0;
        keyEvidence.fill(0);autoKey=proposedKey=keyVotes=0;keyEstablished=false;
        hold.setCurrentAndTargetValue (0);
        clearNotes();
    }
    void detect (float x)
    {
        const float filtered = detectorLow.process (x);
        if (++decimator < decimation) return;
        decimator = 0;
        history[(size_t) detectorPos] = filtered;
        detectorPos = (detectorPos + 1) % 512;
        detectorCount = std::min (512, detectorCount + 1);
        if (++pitchHop < 128 || detectorCount < 512) return;
        pitchHop = 0;
        for (int i = 0; i < 512; ++i) analysis[(size_t) i] = history[(size_t) ((detectorPos + i) % 512)];
        const float ds = (float) sr / (float) decimation;
        const int first = (int) (ds / 1100), last = std::min (255, (int) (ds / 70));
        std::array<float, 256> nsdf {};
        for (int lag = first; lag <= last; ++lag)
        {
            float dot = 0, energy = 0;
            for (int i = 0; i < 512 - lag; ++i)
            {
                const float a = analysis[(size_t) i], b = analysis[(size_t) (i + lag)];
                dot += a * b; energy += a * a + b * b;
            }
            nsdf[(size_t) lag] = energy > 1.e-5f ? 2 * dot / energy : 0;
        }
        confidence = 0;
        for (int lag = first + 1; lag < last; ++lag)
        {
            const float a = nsdf[(size_t) (lag - 1)], b = nsdf[(size_t) lag], c = nsdf[(size_t) (lag + 1)];
            if (b > .88f && b > a && b >= c)
            {
                const float fraction = .5f * (a - c) / (a - 2 * b + c);
                midi = hzToMidi (ds / ((float) lag + fraction)); confidence = b; break;
            }
        }
        updateKey();
    }
    void render (float d, float glassLevel, float haloLevel, float freeze, int key, int scale)
    {
        const float third = AeternaModule::harmonyInterval (midi, key, scale, 2);
        const float fifth = AeternaModule::harmonyInterval (midi, key, scale, scale == 2 ? 3 : 4);
        const float shifts[] { -.065f, .065f, third-.035f, third+.035f, fifth-.025f, fifth+.025f, -12, 12 };
        const float pans[] { -.8f,.8f,-.55f,.55f,-1,1,-.3f,.3f };
        const float harmony = clamp ((d * 127 - 40) / 50, 0, 1) * (confidence > .88f ? 1.f : 0.f);
        const float full = clamp ((d * 127 - 90) / 37, 0, 1) * (confidence > .88f ? 1.f : 0.f);
        float notes[8]{},velocity[8]{};int noteCount=0;
        if(midiMode)
            for(int note=0;note<128&&noteCount<8;++note)
            {
                float vel=0;
                for(int channel=0;channel<16;++channel)if(midiChannel==0||channel+1==midiChannel)vel=std::max(vel,midiVelocity[(size_t)channel][(size_t)note]);
                if(vel>0){notes[noteCount]=(float)note;velocity[noteCount++]=vel;}
            }
        for (auto& v : voices) v.fill (0);
        const float* in[] { input.data() }; float* out[] { shifted.data() };
        for (int v = 0; v < 8; ++v)
        {
            float targetGain = v < 2 ? clamp (d * 4, 0, 1) : v < 6 ? harmony : full;
            if(midiMode)
            {
                targetGain=noteCount>0&&confidence>.88f?clamp(d*2,0,1)*velocity[v%std::max(1,noteCount)]:0;
                if(noteCount>0)midiShifts[(size_t)v]=clamp(notes[v%noteCount]-midi,-36.f,36.f)+(v%2?.025f:-.025f);
            }
            auto& gain = voiceGains[(size_t) v];
            gain += (1 - std::exp (-(float) quantum / (.04f * (float) sr))) * (targetGain - gain);
            auto& shifter = choir[(size_t) v];
            if (gain < .0001f) { if (active[(size_t) v]) shifter.reset(); active[(size_t) v] = false; continue; }
            active[(size_t) v] = true;
            shifter.setTransposeSemitones (midiMode?midiShifts[(size_t)v]:shifts[v]);
            shifter.setFormantSemitones (d * 1.5f + ((float) v - 3.5f) * .14f, true);
            shifter.process (in, quantum, out, quantum);
            for (int c = 0; c < 2; ++c)
            {
                const float pan = std::sqrt (.5f * (1 + (c ? pans[v] : -pans[v])));
                for (int i = 0; i < quantum; ++i) voices[(size_t) c][(size_t) i] += shifted[(size_t) i] * gain * pan * .23f;
            }
        }
        halo.fill (0);
        for (int i = 0; i < quantum; ++i)
            source[(size_t) i] = (input[(size_t) i] * .25f + feedback[(size_t) i] * .38f) * (1 - freeze);
        const float* shimmerIn[] { source.data() };
        for (auto& shifter : shimmer)
        {
            shifter.process (shimmerIn, quantum, out, quantum);
            for (int i = 0; i < quantum; ++i) halo[(size_t) i] += shifted[(size_t) i] / 3;
        }
        static constexpr int major[] { 0,4,7,12,16,19 }, minor[] { 0,3,7,12,15,19 };
        const int* intervals = scale == 1 ? minor : major;
        std::array<float, 6> targets;
        for (int r = 0; r < 6; ++r) targets[(size_t) r] = (float) sr / midiToHz ((scale == 3 && confidence > .88f ? 48.f + std::fmod(midi,12.f) : (float)(48 + key)) + intervals[r]);
        for (int i = 0; i < quantum; ++i)
        {
            float ring[2] {};
            for (int r = 0; r < 6; ++r)
            {
                auto& delay = glass[(size_t) r];
                auto& length = glassDelay[(size_t) r];
                if (length == 0) length = targets[(size_t) r];
                length += .002f * (targets[(size_t) r] - length);
                const float tap = delay.read (length);
                delay.push (input[(size_t) i] * .08f + tap * .91f);
                ring[r % 2] += tap * .4f;
            }
            for (int c = 0; c < 2; ++c)
                roomBuffer.setSample (c, i, (input[(size_t) i] * .16f + voices[(size_t) c][(size_t) i] * .4f
                    + ring[c] * glassLevel * d + halo[(size_t) i] * haloLevel * d) * (1 - freeze));
            for (int c = 0; c < 2; ++c)
                output[(size_t) c][(size_t) i] = voices[(size_t) c][(size_t) i] * .65f + ring[c] * glassLevel * d;
        }
        juce::dsp::AudioBlock<float> block (roomBuffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        room.process (context);
        for (int i = 0; i < quantum; ++i)
        {
            std::array<float, 8> taps;
            float sum = 0;
            for (int r = 0; r < 8; ++r) { taps[(size_t) r] = sustain[(size_t) r].head(); sum += taps[(size_t) r]; }
            float held[2] {};
            for (int r = 0; r < 8; ++r)
            {
                // Householder scattering is energy preserving at Hold=1.
                const float scattered = taps[(size_t) r] - sum * .25f;
                damping[(size_t) r] += .45f * (scattered - damping[(size_t) r]);
                const float recirculate = lerp (damping[(size_t) r], scattered, freeze);
                const float decay = lerp (.78f + d * .19f, 1.f, freeze);
                const float feed = roomBuffer.getSample (r % 2, i) * .16f * (1 - freeze) * (r < 4 ? 1.f : -1.f);
                sustain[(size_t) r].push (clamp (feed + recirculate * decay, -4, 4));
                held[r % 2] += taps[(size_t) r] * (r < 4 ? .5f : -.5f);
            }
            const float tail = .5f * (roomBuffer.getSample (0, i) + roomBuffer.getSample (1, i));
            feedback[(size_t) i] = std::tanh (feedbackLow.process (feedbackHigh.process (tail))) * haloLevel * d;
            for (int c = 0; c < 2; ++c)
                output[(size_t) c][(size_t) i] += roomBuffer.getSample (c, i) * (.35f + d * .8f) + held[c] * (.4f + d);
        }
    }
};

AeternaModule::AeternaModule() { for (int i = 0; i < 8; ++i) params[(size_t) i].store (descriptions[(size_t) i].def); }
AeternaModule::~AeternaModule() = default;
const std::vector<ParamDesc>& AeternaModule::getParamDescs() const { return descriptions; }
float AeternaModule::getParam (int i) const { return i >= 0 && i < 8 ? params[(size_t) i].load() : 0; }
void AeternaModule::setParam (int i, float v)
{
    if (i < 0 || i >= 8 || ! std::isfinite (v)) return;
    const auto& p = descriptions[(size_t) i];
    params[(size_t) i].store (clamp (p.integer ? std::round (v) : v, p.min, p.max));
}
void AeternaModule::prepare (double sr, int, int)
{
    engine = std::make_unique<Engine>();
    engine->prepare (sr, getParam (0) / 127);
}
void AeternaModule::reset() { if (engine) engine->reset(); envelope.store (0); detected.store (0); resolvedKey.store(0); midiNotes.store(0); }
float AeternaModule::getMeter (int i) const { return i == 4 ? midiNotes.load() : i == 3 ? resolvedKey.load() : i == 1 ? tempo.load() : i == 2 ? detected.load() : envelope.load(); }
double AeternaModule::tailLengthSeconds() const { return getParam (5) >= .5f ? std::numeric_limits<double>::infinity() : 30.0; }
void AeternaModule::clearMidi()
{
    if(engine)engine->clearNotes();midiNotes=0;
}
void AeternaModule::handleMidi(const juce::MidiMessage& message)
{
    if(!engine)return;auto& e=*engine;
    const int mode=(int)getParam(6),selected=(int)getParam(7);
    if(mode!=e.midiMode||selected!=e.midiChannel){clearMidi();e.midiMode=mode;e.midiChannel=selected;}
    if(!mode)return;
    const int ch=message.getChannel()-1;if(ch<0||ch>=16||(selected!=0&&selected!=ch+1))return;
    if(message.isNoteOn()) {e.keyDown[(size_t)ch][(size_t)message.getNoteNumber()]=true;e.midiVelocity[(size_t)ch][(size_t)message.getNoteNumber()]=message.getFloatVelocity();}
    else if(message.isNoteOff())
    {
        e.keyDown[(size_t)ch][(size_t)message.getNoteNumber()]=false;
        if(!e.pedal[(size_t)ch])e.midiVelocity[(size_t)ch][(size_t)message.getNoteNumber()]=0;
    }
    else if(message.isAllNotesOff()||message.isAllSoundOff()) {e.midiVelocity[(size_t)ch].fill(0);e.keyDown[(size_t)ch].fill(false);e.pedal[(size_t)ch]=false;}
    else if(message.isController()&&(message.getControllerNumber()==64||message.getControllerNumber()==121))
    {
        e.pedal[(size_t)ch]=message.getControllerNumber()==64&&message.getControllerValue()>=64;
        if(!e.pedal[(size_t)ch])for(int n=0;n<128;++n)if(!e.keyDown[(size_t)ch][(size_t)n])e.midiVelocity[(size_t)ch][(size_t)n]=0;
    }
    int count=0;for(int n=0;n<128;++n){bool active=false;for(int c=0;c<16;++c)active|=e.midiVelocity[(size_t)c][(size_t)n]>0;count+=active?1:0;}midiNotes=(float)std::min(8,count);
}
void AeternaModule::processWithMidi(juce::AudioBuffer<float>& buffer,const juce::MidiBuffer& midi,int offset)
{
    if(engine&&((int)getParam(6)!=engine->midiMode||(int)getParam(7)!=engine->midiChannel))
    {clearMidi();engine->midiMode=(int)getParam(6);engine->midiChannel=(int)getParam(7);}
    if(getParam(6)<.5f){process(buffer);return;}
    int start=0;
    auto segment=[&](int end){if(end<=start)return;float* ptr[2]{};const int channels=std::min(2,buffer.getNumChannels());for(int c=0;c<channels;++c)ptr[c]=buffer.getWritePointer(c,start);juce::AudioBuffer<float> part(ptr,channels,end-start);process(part);start=end;};
    for(auto it=midi.findNextSamplePosition(offset);it!=midi.end();++it)
    {
        const auto event=*it;if(event.samplePosition>=offset+buffer.getNumSamples())break;
        if(event.numBytes>3)continue;
        segment(std::max(start,event.samplePosition-offset));handleMidi(event.getMessage());
    }
    segment(buffer.getNumSamples());
}
void AeternaModule::process (juce::AudioBuffer<float>& buffer)
{
    if (! engine || buffer.getNumChannels() == 0) return;
    juce::ScopedNoDenormals noDenormals;
    auto& e = *engine;
    e.devotion.setTargetValue (getParam (0) / 127); e.glassAmount.setTargetValue (getParam (1) / 100);
    e.haloAmount.setTargetValue (getParam (2) / 100); e.hold.setTargetValue (getParam (5));
    const int key = (int) getParam (3), scale = (int) getParam (4);
    e.analysisScale = scale;
    const int channels = std::min (2, buffer.getNumChannels());
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float d = e.devotion.getNextValue(), gl = e.glassAmount.getNextValue(), ha = e.haloAmount.getNextValue(), freeze = e.hold.getNextValue();
        const float left = buffer.getSample (0, i), right = buffer.getSample (channels - 1, i);
        const float mono = .5f * (left + right);
        e.detect (mono);
        e.env += (std::abs (mono) > e.env ? .01f : .0002f) * (std::abs (mono) - e.env);
        const float clean = e.preHigh.process (mono);
        const float warm = std::tanh (clean * (1 + d)) / (1 + d);
        e.input[(size_t) e.pos] = lerp (warm, e.preAir.process (warm), d) * d;
        // Parallel wet predelay is intentional; the original stereo signal stays immediate.
        for (int c = 0; c < channels; ++c)
        {
            const float wet = channels == 1 ? .5f * (e.output[0][(size_t) e.pos] + e.output[1][(size_t) e.pos]) : e.output[(size_t) c][(size_t) e.pos];
            const float dry = buffer.getSample (c, i);
            buffer.setSample (c, i, d == 0 ? dry : dry * (1 - .35f * d) + std::tanh (wet) * d);
        }
        if (++e.pos == quantum) { e.render (d, gl, ha, freeze, key == 12 ? e.autoKey : key, scale); e.pos = 0; }
    }
    resolvedKey.store((float)(key == 12 ? e.autoKey : key));
    envelope.store (e.env); detected.store (e.confidence > .88f ? e.midi : 0);
    tempo.store ((float) (std::isfinite (currentBpm) && currentBpm > 0 ? currentBpm : 120));
}
}
