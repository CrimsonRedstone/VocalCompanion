#pragma once

#include <JuceHeader.h>
#include "../DSP/WaveShaper.h"

namespace vc
{

class WaveShaperVisualizer : public juce::Component
{
public:
    WaveShaperVisualizer(const WaveShaperDSP& dspRef) : dsp(dspRef)
    {
        setOpaque(true);
    }

    void refresh()
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff181a1f));

        // Grid lines (center crosshairs and borders)
        g.setColour(juce::Colour(0xff2d3139));
        g.drawRect(bounds, 1.0f);
        const float dashPattern[] { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float> (bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY()), dashPattern, 2, 1.0f);
        g.drawDashedLine(juce::Line<float> (bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom()), dashPattern, 2, 1.0f);

        // Linear neutral reference line (y = x)
        g.setColour(juce::Colour(0x33ffffff));
        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 1.0f);

        // Render Transfer Curve
        juce::Path curvePath;
        const int steps = (int)bounds.getWidth();
        if (steps <= 0)
            return;

        for (int i = 0; i <= steps; ++i)
        {
            float normX = (float)i / (float)steps;
            float inSample = normX * 2.0f - 1.0f; // [-1.0 to +1.0]
            float outSample = dsp.evaluateShape(inSample); // [-1.0 to +1.0]

            float px = bounds.getX() + normX * bounds.getWidth();
            // Flip y because screen coordinate y=0 is top
            float py = bounds.getY() + (1.0f - (outSample + 1.0f) * 0.5f) * bounds.getHeight();

            if (i == 0)
                curvePath.startNewSubPath(px, py);
            else
                curvePath.lineTo(px, py);
        }

        // Draw path with a smooth gradient
        juce::ColourGradient grad(juce::Colour(0xffff9900), bounds.getX(), bounds.getBottom(),
                                  juce::Colour(0xffff4444), bounds.getRight(), bounds.getY(), false);
        g.setGradientFill(grad);
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved));
    }

private:
    const WaveShaperDSP& dsp;
};

} // namespace vc