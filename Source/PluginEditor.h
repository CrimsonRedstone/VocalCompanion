#pragma once
#include "PluginProcessor.h"
#include "VcLookAndFeel.h"
#include "UI/TopBar.h"
#include "UI/RackComponent.h"
#include "UI/PaletteDock.h"

class VocalCompanionEditor : public juce::AudioProcessorEditor,
                             public juce::DragAndDropContainer
{
public:
    explicit VocalCompanionEditor (VocalCompanionProcessor&);
    ~VocalCompanionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applySkin();
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void updateWindowIcon();
    VocalCompanionProcessor& proc;
    VcLookAndFeel lnf;
    juce::Component root;
    TopBar top;
    RackComponent rack;
    EffectList list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompanionEditor)
};
