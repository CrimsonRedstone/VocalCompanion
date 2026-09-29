#pragma once
#include "VcLookAndFeel.h"
#include "DSP/Common.h"
#include "AudioVisuals.h"

class IconButton : public juce::Button
{
public:
    enum Kind { Plus, Swap, Close };
    explicit IconButton (Kind k, juce::Colour ac = juce::Colour (0xff3ec8e0))
        : juce::Button ({}), kind (k), accent (ac)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? accent.darker (0.2f) : (over ? juce::Colour (0xff2a3844) : juce::Colour (0xff1c242c)));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (kind == Close ? juce::Colour (0xffff6a82) : accent);
        auto c = r.getCentre();
        const float arm = juce::jmax (2.4f, r.getWidth() * 0.28f);
        if (kind == Plus)
        {
            g.drawLine (c.x - arm, c.y, c.x + arm, c.y, 1.5f);
            g.drawLine (c.x, c.y - arm, c.x, c.y + arm, 1.5f);
        }
        else if (kind == Swap)
        {
            juce::Path p;
            p.startNewSubPath (c.x - 4.5f, c.y - 2.2f);
            p.lineTo (c.x + 2.5f, c.y - 2.2f);
            p.lineTo (c.x + 0.5f, c.y - 4.5f);
            g.strokePath (p, juce::PathStrokeType (1.5f));
            juce::Path q;
            q.startNewSubPath (c.x + 4.5f, c.y + 2.2f);
            q.lineTo (c.x - 2.5f, c.y + 2.2f);
            q.lineTo (c.x - 0.5f, c.y + 4.5f);
            g.strokePath (q, juce::PathStrokeType (1.5f));
        }
        else
        {
            g.drawLine (c.x - 3.2f, c.y - 3.2f, c.x + 3.2f, c.y + 3.2f, 1.6f);
            g.drawLine (c.x + 3.2f, c.y - 3.2f, c.x - 3.2f, c.y + 3.2f, 1.6f);
        }
    }
    Kind kind;
    juce::Colour accent;
};

class ParamKnob : public juce::Component, private juce::Timer
{
public:
    ParamKnob (vc::VcModule& m, int paramIndex, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::AudioProcessorParameter* hostParam { nullptr };

private:
    void timerCallback() override;
    vc::VcModule& module;
    int index;
    Knob knob;
    juce::Label name, value;
};

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::function<float()> src, juce::Colour c = juce::Colour (0xff3ec8e0));
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    std::function<float()> source;
    juce::Colour colour;
    float shown { 0 };
};

class CurveView : public juce::Component, private juce::Timer
{
public:
    explicit CurveView (vc::VcModule& m);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { visual.update(); repaint(); }
    vc::VcModule& module;
    AudioVisuals visual;
};

/** Graphic EQ pad: drag numbered dots (Y = gain, X = freq when movable). */
class EqPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    EqPad (vc::VcModule& m, bool freqMovable);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override { visual.update(); repaint(); }
    vc::VcModule& module;
    AudioVisuals visual;
    bool freqMovable;
    int drag { -1 };
    int nodeCount() const { return freqMovable ? 3 : 5; }
    juce::Point<float> nodePos (int i, juce::Rectangle<float> r) const;
};
