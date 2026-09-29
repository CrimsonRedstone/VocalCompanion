#pragma once
#include <JuceHeader.h>
#include "Chain.h"
#include "DSP/Dynamics.h"

class VocalCompanionProcessor : public juce::AudioProcessor
{
public:
    VocalCompanionProcessor();
    ~VocalCompanionProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vocal Companion"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    vc::Chain& getChain() { return chain; }
    const vc::Chain& getChain() const { return chain; }

    std::atomic<float> masterInDb  { 0.0f };
    std::atomic<float> masterOutDb { 0.0f };
    std::atomic<float> inputPeak   { 0.0f };
    std::atomic<float> outputPeak  { 0.0f };
    vc::Comparison rackComparison;
    std::atomic<float> rackLatencyMs {0};
    int skinIndex { 0 };
    float skinHue() const { return vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, skinIndex)].hue; }

    juce::String currentPresetName { "Default Rack" };

    void loadStateTree (const juce::ValueTree&);
    void loadPresetTree (const juce::ValueTree&);
    juce::ValueTree saveStateTree() const;
    void selectModuleSnapshot(vc::VcModule&,int);
    void selectRackSnapshot(int);
    void initialiseRackSnapshots();
    void randomizeAllParams();
    void randomizeEverything();
    juce::StringArray favorites;
    bool isFavorite (const juce::String& n) const { return favorites.contains (n); }
    void toggleFavorite (const juce::String& n);

    mutable juce::CriticalSection chainLock;

    static constexpr int kHostParamCount = 80;
    juce::AudioProcessorParameter* hostParamFor (vc::VcModule*, int paramIndex);
    void rebindHostParams();

private:
    juce::ValueTree captureRackSnapshot() const;
    std::array<juce::ValueTree,2> rackSnapshots;
    void notifySnapshotParams(vc::VcModule* only = nullptr);
    struct BindableParam final : public juce::AudioProcessorParameter
    {
        juce::String label { "Unused" };
        std::atomic<float> norm { 0.0f };
        vc::VcModule* target { nullptr };
        int targetIndex { -1 };
        float minV { 0 }, maxV { 1 }, defV { 0 };

        juce::String getName (int) const override { return label; }
        juce::String getLabel() const override { return {}; }
        float getValue() const override { return norm.load(); }
        void setValue (float v) override
        {
            norm.store (v);
            if (target != nullptr)
                target->setParam (targetIndex, minV + v * (maxV - minV));
        }
        float getDefaultValue() const override
        {
            return (maxV > minV) ? (defV - minV) / (maxV - minV) : 0.0f;
        }
        int getNumSteps() const override { return 0; }
        juce::String getText (float v, int) const override
        {
            return juce::String (minV + v * (maxV - minV), 2);
        }
        float getValueForText (const juce::String& t) const override
        {
            const float den = t.getFloatValue();
            return (maxV > minV) ? juce::jlimit (0.0f, 1.0f, (den - minV) / (maxV - minV)) : 0.0f;
        }
        bool isAutomatable() const override { return target != nullptr; }
    };

    vc::Chain chain;
    BindableParam* hostParams[kHostParamCount] {};
    juce::SmoothedValue<float> inSm, outSm;
    double sampleRate { 44100.0 };
    int blockSize { 512 };
    bool wasPlaying {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompanionProcessor)
};
