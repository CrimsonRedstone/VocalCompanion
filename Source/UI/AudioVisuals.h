#pragma once
#include "DSP/Common.h"

// All analysis and drawing happens on the message thread. Traces are a visual
// guide, not a calibrated loudness meter or a latency-aligned null comparison.
class AudioVisuals
{
public:
    explicit AudioVisuals (vc::VcModule& m) : module (m), tap (m.visual) { ++tap->readers; }
    ~AudioVisuals() { --tap->readers; }
    void update()
    {
        vc::VisualTap::Frame candidate = frame;
        const bool fresh = tap->read (candidate) && !module.isBypassed();
        if (fresh) { frame = candidate; staleTicks = 0; }
        else staleTicks = std::min (100, staleTicks + 1);
        // Hosts may deliver blocks longer than one UI tick. Hold the last frame
        // briefly instead of blinking between blocks, then settle when stopped.
        live = !module.isBypassed() && frame.serial != 0 && staleTicks <= 6;
        float in = 0, out = 0, inPeak = 0, outPeak = 0;
        if (live)
        {
            for (int c = 0; c < 4; ++c)
            {
                float power = 0;
                for (auto value : frame.wave[(size_t)c])
                {
                    power += value*value;
                    if(c<2)inPeak=std::max(inPeak,std::abs(value));else outPeak=std::max(outPeak,std::abs(value));
                }
                power = std::sqrt (power / vc::VisualTap::size);
                if (c < 2) in = juce::jmax (in, power); else out = juce::jmax (out, power);
            }
        }
        energy += (std::min (1.0f, out * 5) - energy) * (fresh ? .4f : .15f);
        for (int i = 0; i < 95; ++i) { input[(size_t)i] = input[(size_t)i+1]; output[(size_t)i] = output[(size_t)i+1]; reduction[(size_t)i] = reduction[(size_t)i+1]; peakInput[(size_t)i]=peakInput[(size_t)i+1]; peakOutput[(size_t)i]=peakOutput[(size_t)i+1]; }
        input[95] = level (in); output[95] = level (out);
        peakInput[95]=inPeak;peakOutput[95]=outPeak;
        reduction[95] = live ? juce::jlimit (0.0f, 1.0f, module.getMeter()) : 0;
        if (!fresh && live) return;
        for (int side = 0; side < 2; ++side)
        {
            std::array<float, 2048> bins {};
            if (fresh)
            {
                // Choose the stronger channel; summing would hide anti-phase audio.
                int ch = side * 2;
                float a = 0, b = 0;
                for (int i = 0; i < 1024; ++i) { a += std::abs (frame.wave[(size_t)ch][(size_t)i]); b += std::abs (frame.wave[(size_t)ch+1][(size_t)i]); }
                if (b > a) ++ch;
                for (int i = 0; i < 1024; ++i)
                    bins[(size_t)i] = frame.wave[(size_t)ch][(size_t)i] * (.5f - .5f * std::cos (vc::kTwoPi * i / 1023.0f));
                fft.performFrequencyOnlyForwardTransform (bins.data());
            }
            for (int i = 0; i < 96; ++i)
            {
                const float hz = 40 * std::pow (500.0f, i / 95.0f);
                const float bin = juce::jlimit (1.0f, 510.0f, hz * 1024.0f / (float) frame.sampleRate);
                const int lo = (int)bin;
                const float v = fresh ? level (vc::lerp (bins[(size_t)lo], bins[(size_t)lo+1], bin-lo) / 256.0f) : 0;
                auto& shown = spectrum[(size_t)side][(size_t)i];
                shown += (v-shown) * (v > shown ? .6f : .18f);
            }
        }
    }
    static float level (float x) { return juce::jlimit (0.0f, 1.0f, (vc::gainToDb (x) + 72) / 72); }
    static void path (juce::Graphics& g, juce::Rectangle<float> r, const std::array<float,96>& values, juce::Colour colour, bool filled)
    {
        juce::Path p;
        for (int i = 0; i < 96; ++i)
        {
            float x = r.getX() + r.getWidth()*i/95.0f, y = r.getBottom()-r.getHeight()*values[(size_t)i];
            if (i == 0) p.startNewSubPath (x,y); else p.lineTo (x,y);
        }
        if (filled)
        {
            auto area = p; area.lineTo (r.getBottomRight()); area.lineTo (r.getBottomLeft()); area.closeSubPath();
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (.32f), r.getTopLeft(), colour.withAlpha (.01f), r.getBottomLeft(), false));
            g.fillPath (area);
        }
        g.setColour (colour.withAlpha (.12f)); g.strokePath (p, juce::PathStrokeType (5));
        g.setColour (colour); g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    void spectra (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ac)
    {
        path (g, r, spectrum[0], juce::Colour (0xff87929e).withAlpha (.5f), false);
        path (g, r, spectrum[1], ac.withAlpha (.75f), true);
    }
    void history (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ac)
    {
        path (g, r, input, juce::Colour (0xffaab8c9).withAlpha (.48f), false);
        path (g, r, output, ac, true);
    }
    void scope (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ac)
    {
        // Each waveform is triggered at a rising zero crossing. This compares
        // period/shape without claiming sample-aligned subtraction through a
        // processor with latency. Both traces use the same amplitude scale.
        int channels[2] {0,2};
        float maximum=.1f;
        for(int side=0;side<2;++side)
        {
            float powers[2] {};
            for(int c=0;c<2;++c)for(auto value:frame.wave[(size_t)(side*2+c)])powers[c]+=value*value;
            channels[side]=side*2+(powers[1]>powers[0]?1:0);
            for(auto value:frame.wave[(size_t)channels[side]])maximum=std::max(maximum,std::abs(value));
        }
        const int span=juce::jlimit(256,768,(int)(frame.sampleRate*.012));
        for(int side=0;side<2;++side)
        {
            const auto& wave=frame.wave[(size_t)channels[side]];
            int start=0;
            for(int i=1;i<1024-span;++i)if(wave[(size_t)i-1]<=0 && wave[(size_t)i]>0){start=i;break;}
            juce::Path p;
            for(int i=0;i<256;++i)
            {
                const float value=live?wave[(size_t)(start+i*(span-1)/255)]:0;
                const float x=r.getX()+r.getWidth()*i/255.f,y=r.getCentreY()-value/maximum*r.getHeight()*.44f;
                if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);
            }
            if(side){g.setColour(ac.withAlpha(.12f));g.strokePath(p,juce::PathStrokeType(5));}
            g.setColour(side?ac:juce::Colour(0xffc0c9d7).withAlpha(.5f));
            g.strokePath(p,juce::PathStrokeType(side?1.8f:.9f));
        }
    }
    vc::VcModule& module;
    std::shared_ptr<vc::VisualTap> tap;
    vc::VisualTap::Frame frame;
    std::array<float,96> input {}, output {}, reduction {}, peakInput {}, peakOutput {};
    std::array<std::array<float,96>,2> spectrum {};
    float energy = 0;
    bool live = false;
private:
    int staleTicks = 100;
    juce::dsp::FFT fft { 10 };
};
