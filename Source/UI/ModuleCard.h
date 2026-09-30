#pragma once
#include "UI/Controls.h"
#include "UI/AeternaView.h"
#include "Chain.h"
#include "UI/ComparisonControl.h"

class ModuleCard : public juce::Component
{
public:
    ModuleCard (vc::VcModule& m, class VocalCompanionProcessor& proc,
                std::function<void()> onDelete, std::function<void()> onBypass, bool expanded = false);
    ~ModuleCard() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    int preferredHeight() const;
    static constexpr int preferredWidth = 224;

    vc::VcModule& module;
    std::function<void()> onDuplicate;
    std::function<void (ModuleCard*)> onDragStart;
    std::function<void (int screenX, int screenY)> onReplace;

private:
    VocalCompanionProcessor& processor;
    bool expandedView=false;
    void openFull();
    std::unique_ptr<juce::DocumentWindow> fullWindow;
    juce::TextButton duplicate {"D"};
    IconButton full {IconButton::Expand};
    juce::Label advancedTitle;
    int coreKnobs=0;
    std::unique_ptr<ComparisonControl> compare;
    juce::TextButton bypass { "BYP" };
    IconButton replace { IconButton::Swap };
    IconButton close { IconButton::Close };
    juce::TextButton openGui { "Open GUI" };
    std::unique_ptr<juce::Component> graph;
    juce::OwnedArray<ParamKnob> knobs;
    juce::OwnedArray<ParamChoice> choices;
    vc::ModuleColour colours;
    int knobColumns() const;
};
