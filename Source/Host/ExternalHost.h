#pragma once
#include "DSP/Common.h"

namespace vc
{

/** Wraps a third-party VST3 / AU / CLAP instance inside the rack.
    Scanning and editor hosting live on the message thread; process() is
    realtime-safe once the instance is prepared. */
class ExternalModule final : public VcModule
{
public:
    ExternalModule();
    ~ExternalModule() override;

    ModuleType getType() const override { return ModuleType::External; }
    juce::String getDisplayName() const override;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    const std::vector<ParamDesc>& getParamDescs() const override { return descs; }
    float getParam (int) const override { return 0; }
    void  setParam (int, float) override {}

    juce::ValueTree toValueTree() const override;
    void fromValueTree (const juce::ValueTree&) override;

    bool loadFromFile (const juce::File& file);
    bool loadFromDescription (const juce::PluginDescription& desc);
    void closePlugin();

    juce::AudioPluginInstance* getInstance() { return instance.get(); }
    const juce::PluginDescription& getDescription() const { return description; }
    bool isLoaded() const { return instance != nullptr; }

    void openEditor();
    void closeEditor();

private:
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::PluginDescription description;
    juce::MemoryBlock pluginState;
    double sampleRate { 44100.0 };
    int blockSize { 512 };
    int numCh { 2 };
    juce::MidiBuffer emptyMidi;
    std::unique_ptr<juce::DocumentWindow> editorWindow;

    inline static const std::vector<ParamDesc> descs {};
};

void showPluginScannerDialog (std::function<void (juce::PluginDescription)> onChosen);

} // namespace vc
