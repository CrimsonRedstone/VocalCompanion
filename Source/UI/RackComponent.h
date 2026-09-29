#pragma once
#include "UI/ModuleCard.h"
#include "PluginProcessor.h"

class RackComponent : public juce::Component,
                      public juce::DragAndDropTarget
{
public:
    explicit RackComponent (VocalCompanionProcessor&);
    ~RackComponent() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void rebuild();
    void revealLast();
    void revealFirst() { viewport.setViewPosition (0, 0); }
    void showAdd (int at, int sx, int sy);

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;

    std::function<void()> onChanged;

private:
    VocalCompanionProcessor& proc;
    juce::OwnedArray<ModuleCard> cards;
    juce::OwnedArray<IconButton> pluses;
    juce::Viewport viewport;
    juce::Component strip;
    int dropIndex { -1 };
    int indexForX (int x) const;
    void insertType (vc::ModuleType type, int at);
    void replaceType (int at, vc::ModuleType type);
    void showReplace (int at, int sx, int sy);
};
