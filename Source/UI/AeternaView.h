#pragma once
#include "DSP/Aeterna.h"

class AeternaView final : public juce::Component, private juce::Timer
{
public:
    explicit AeternaView (vc::VcModule&);
    void paint (juce::Graphics&) override;
    static juce::Rectangle<int> frameBounds (int velocity)
    {
        velocity = juce::jlimit (0, 127, velocity);
        return { velocity % 16 * 128, velocity / 16 * 192, 128, 192 };
    }
private:
    struct Atlases;
    juce::SharedResourcePointer<Atlases> atlases;
    vc::VcModule& module;
    double lastTime = 0, phase = 0;
    float velocity = 0, glow = 0, glass = 0, shimmer = 0;
    void timerCallback() override;
};

class ParamChoice final : public juce::Component, private juce::Timer
{
public:
    ParamChoice (vc::VcModule&, int, juce::AudioProcessorParameter*);
    std::function<void()> onBeginEdit;
    void resized() override;
private:
    vc::VcModule& module;
    int index;
    juce::AudioProcessorParameter* host;
    juce::ComboBox choice;
    juce::Label label;
    void timerCallback() override;
};
