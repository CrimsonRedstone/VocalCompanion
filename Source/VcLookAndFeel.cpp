#include "VcLookAndFeel.h"

VcLookAndFeel::VcLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff07090c));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff16181f));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8eaee));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a2e38));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff12141a));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe8eaee));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a3a44));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c2028));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eaee));
    setColour (juce::Label::textColourId, juce::Colour (0xffc8ccd4));
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe8eaee));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void VcLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float sliderPos, float rotaryStart, float rotaryEnd,
                                      juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStart + sliderPos * (rotaryEnd - rotaryStart);
    auto ac = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (ac.isTransparent()) ac = accent;

    g.setColour (juce::Colour (0xff0c0e12));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2, radius * 2);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0,
                       rotaryStart, rotaryEnd, true);
    g.setColour (juce::Colour (0xff2a3038));
    g.strokePath (arc, juce::PathStrokeType (2.4f));

    juce::Path val;
    val.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0,
                       rotaryStart, angle, true);
    g.setColour (ac);
    g.strokePath (val, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const auto inner = radius * 0.62f;
    g.setColour (juce::Colour (0xff1a1e26));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2, inner * 2);

    juce::Path needle;
    needle.addRoundedRectangle (-1.1f, -inner + 1.0f, 2.2f, inner * 0.72f, 1.0f);
    g.setColour (ac.brighter (0.2f));
    g.fillPath (needle, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void VcLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                      bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (b.getToggleState() ? accent.withAlpha (0.85f)
                                    : juce::Colour (highlighted ? 0xff2a3038 : 0xff1a1e24));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0x33000000));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (b.getToggleState() ? juce::Colour (0xff071014) : juce::Colour (0xffc8ccd4));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font VcLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")); }
juce::Font VcLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (13.0f)); }
juce::Font VcLookAndFeel::getLabelFont (juce::Label&) { return juce::Font (juce::FontOptions (10.0f)); }

void VcLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.setColour (chrome);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
    g.setColour (accent.withAlpha (0.5f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
}

juce::PopupMenu::Options VcLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label& label)
{
    juce::Component* host = nullptr;
    for (auto* p = box.getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (p->getName() == "vc-root")
            host = p;

    auto opts = juce::PopupMenu::Options()
        .withTargetComponent (&box)
        .withMinimumWidth (juce::jmax (box.getWidth(), 220))
        .withMaximumNumColumns (1)
        .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards)
        .withItemThatMustBeVisible (box.getSelectedId())
        .withStandardItemHeight (juce::jmax (22, label.getHeight()));
    if (host != nullptr)
        opts = opts.withParentComponent (host);
    return opts;
}

void VcLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                const juce::String& name)
{
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
    g.drawText (name, area.reduced (10, 0), juce::Justification::centredLeft);
}

void VcLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                       bool isSeparator, bool isActive, bool isHighlighted,
                                       bool, bool, const juce::String& text,
                                       const juce::String&, const juce::Drawable*,
                                       const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (juce::Colour (0xff2a3038));
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.toFloat().reduced (4.0f, 2.0f);
    auto ac = textColour != nullptr ? *textColour : accent;
    if (isHighlighted && isActive)
    {
        g.setColour (ac.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    g.setColour (ac);
    g.fillRect (r.getX(), r.getY() + 4.0f, 3.0f, r.getHeight() - 8.0f);
    g.setColour (isActive ? juce::Colour (0xffe8eaee) : juce::Colour (0xff6a7080));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (text, area.translated (12, 0), juce::Justification::centredLeft);
}

Knob::Knob (const juce::String& name, juce::Colour accentCol) : juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox)
{
    setName (name);
    setColour (rotarySliderFillColourId, accentCol);
    setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    setPopupMenuEnabled (false);
    setDoubleClickReturnValue (true, 0.0);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onHostMenu)
            onHostMenu (e.getScreenPosition());
        return;
    }
    if (onGesture) onGesture (true);
    juce::Slider::mouseDown (e);
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    if (! e.mods.isPopupMenu() && onGesture)
        onGesture (false);
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (isDoubleClickReturnEnabled())
        setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
}

SmallToggle::SmallToggle (const juce::String& name) : juce::ToggleButton (name) {}
