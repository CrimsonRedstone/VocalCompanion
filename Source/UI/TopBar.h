#pragma once
#include "PluginProcessor.h"
#include "UI/Controls.h"
#include "UI/ComparisonControl.h"

class TopBar : public juce::Component
{
public:
    explicit TopBar (VocalCompanionProcessor&, std::function<void()> onChainChanged);
    void resized() override;
    void paint (juce::Graphics&) override;
    void rebuildPresetList(); // Refreshes the current name and master controls.
    std::function<void()> onBrowsePresets;

private:
    VocalCompanionProcessor& proc;
    std::unique_ptr<ComparisonControl> compare;
    juce::TextButton undo {"Undo"}, redo {"Redo"};
    juce::TextButton presetButton;
    juce::TextButton saveBtn { "Save As" }, loadBtn { "Load" };
    juce::Slider inGain, outGain;
    juce::HyperlinkButton support {
        "Freeware by Crimson Redstone",
        juce::URL ("https://crimsonredstone.bandcamp.com") };
    LevelMeter inMeter, outMeter;
    std::function<void()> chainChanged;
};
