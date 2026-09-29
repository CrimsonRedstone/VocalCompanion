#pragma once
#include "Common.h"

namespace vc
{
class AeternaModule final : public VcModule
{
public:
    AeternaModule();
    ~AeternaModule() override;
    ModuleType getType() const override { return ModuleType::Aeterna; }
    void prepare (double, int, int) override;
    void process (juce::AudioBuffer<float>&) override;
    void reset() override;
    void handleMidi(const juce::MidiMessage&) override;
    void clearMidi() override;
    void processWithMidi(juce::AudioBuffer<float>&,const juce::MidiBuffer&,int) override;
    const std::vector<ParamDesc>& getParamDescs() const override;
    float getParam (int) const override;
    void setParam (int, float) override;
    float getMeter (int = 0) const override;
    int getNumMeters() const override { return 5; } // input envelope, tempo, detected MIDI, resolved key
    double tailLengthSeconds() const override;
    static float harmonyInterval (float midi, int key, int scale, int degree);
private:
    struct Engine;
    std::unique_ptr<Engine> engine;
    std::array<std::atomic<float>, 8> params;
    std::atomic<float> envelope { 0 }, tempo { 120 }, detected { 0 }, resolvedKey { 0 }, midiNotes { 0 };
};
}
