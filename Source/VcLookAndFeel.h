#pragma once
#include <JuceHeader.h>
#include "DSP/Common.h"

class VcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VcLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStart, float rotaryEnd,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcut, const juce::Drawable*,
                            const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&,
                                     const juce::String&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;

    juce::Colour accent { juce::Colour (0xff3ec8e0) };
    juce::Colour panel  { juce::Colour (0xff101218) };
    juce::Colour chrome { juce::Colour (0xff181b22) };
};

class Knob : public juce::Slider
{
public:
    Knob (const juce::String& name, juce::Colour accent);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void (juce::Point<int>)> onHostMenu;
    std::function<void (bool /*begin*/)> onGesture;
};

class SmallToggle : public juce::ToggleButton
{
public:
    SmallToggle (const juce::String& name);
};
