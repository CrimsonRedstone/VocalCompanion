#pragma once
#include <JuceHeader.h>
#include "Chain.h"
#include "Presets.h"

class EffectList : public juce::Component, public juce::DragAndDropTarget, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit EffectList (class VocalCompanionProcessor&);
    ~EffectList() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { stopTimer(); dropIndex = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void paintOverChildren (juce::Graphics&) override;
    void showPresets();
    bool isCollapsed() const { return collapsed; }
    int preferredWidth() const { return collapsed ? 24 : 300; }
    std::function<void()> onCollapseChanged, onAddVst, onChainChanged, onSkinChanged, onRevealAdded;
    std::function<void (int sx, int sy)> onAddModule;

private:
    enum Tab { Effects, Presets, Fun };
    enum Filter { All, Factory, Saved, Favorites };
    VocalCompanionProcessor& proc;
    juce::TextButton collapse { "<" };
    juce::TextButton tabFx { "Effects" }, tabPresets { "Presets" }, tabFun { "Fun" };
    juce::TextButton addEffect { "+ Effect" }, addVst { "+ External" };
    juce::TextButton addAeterna { "+ Aeterna" };
    juce::TextButton randomPreset { "Random Factory Preset" };
    juce::TextButton randomizeAll { "Randomize Rack" };
    juce::OwnedArray<juce::TextButton> sourceButtons, tagButtons, skinButtons;
    juce::TextEditor search;
    juce::ListBox rows;
    juce::Label countLabel, details, skinHeading;
    std::vector<vc::RackPresetEntry> entries;
    std::vector<int> visible;
    bool collapsed { false };
    Tab tab { Effects };
    Filter filter { All };
    int soundFilter { 0 };
    juce::String inspectedKey;
    juce::Rectangle<int> detailsBounds;
    void setTab (Tab);
    void rebuildResults();
    void applyEntry (const vc::RackPresetEntry&);
    bool isFavorite (const vc::RackPresetEntry&) const;
    void toggleFavorite (const vc::RackPresetEntry&);
    void updateDetails (int row);
    void updateColours();
    int dropIndex { -1 }, dragY { 0 };
    void timerCallback() override;
    int insertionAt (int y) const;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int row) override;
    void returnKeyPressed (int row) override;
};
