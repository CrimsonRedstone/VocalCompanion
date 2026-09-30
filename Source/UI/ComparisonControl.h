#pragma once
#include "DSP/Comparison.h"

class SnapshotButton : public juce::TextButton
{
public:
    void paintButton(juce::Graphics& g,bool over,bool) override
    {
        const bool selected=getToggleState();auto r=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(juce::Colour(selected?0xff101720:over?0xff52606e:0xff3c4652));
        g.fillRoundedRectangle(r,4);g.setColour(juce::Colour(selected?0xff7ddbed:0xff76818e));
        g.drawRoundedRectangle(r,4,selected?1.4f:.6f);
        g.setColour(juce::Colour(selected?0xfff4f7fb:0xffb1b8c2));
        g.setFont(juce::Font(juce::FontOptions(11.f).withStyle(selected?"Bold":"Regular")));
        g.drawText(getButtonText(),getLocalBounds(),juce::Justification::centred);
    }
};

class ComparisonControl : public juce::Component, private juce::Timer
{
public:
    ComparisonControl(vc::Comparison& c,std::function<float()> load,std::function<float()> delay,std::function<void(int)> select)
        : comparison(c),cpu(std::move(load)),latency(std::move(delay)),selectSlot(std::move(select))
    {
        for(int i=0;i<2;++i)
        {
            addAndMakeVisible(buttons[i]);buttons[i].setButtonText(i==0?"A":"B");
            buttons[i].setTooltip("Recall settings "+juce::String(i==0?"A":"B")+". Edits stay in the selected slot. Both slots start from the initial or loaded preset settings.");
            buttons[i].onClick=[this,i]{if(comparison.mode.load()!=i)selectSlot(i);refresh();};
        }
        addAndMakeVisible(match);match.setButtonText("Match");match.setTooltip("Match B's weighted level to A after playing both. Disable to compare their original output levels.");
        match.onClick=[this]{comparison.matchEnabled=!comparison.matchEnabled.load();refresh();};
        addAndMakeVisible(status);status.setFont(juce::Font(juce::FontOptions(9.f)));status.setInterceptsMouseClicks(false,false);
        status.setTooltip("CPU: smoothed processing time relative to the audio time budget. Latency: active settings' reported processing delay.");
        refresh();startTimerHz(8);
    }
    void resized() override
    {
        auto row=getLocalBounds();auto controls=row.removeFromLeft(58);
        auto top=controls.removeFromTop(16);buttons[0].setBounds(top.removeFromLeft(29).reduced(1));buttons[1].setBounds(top.reduced(1));
        match.setBounds(controls.reduced(1));status.setBounds(row);
    }
private:
    void timerCallback() override {refresh();}
    void refresh()
    {
        for(int i=0;i<2;++i)buttons[i].setToggleState(comparison.mode.load()==i,juce::dontSendNotification);
        match.setToggleState(comparison.matchEnabled.load(),juce::dontSendNotification);match.setEnabled(comparison.available.load());
        juce::String text=cpu?"CPU "+juce::String(cpu(),1)+"% | ":"Rack | ";
        text+=juce::String(latency(),latency()<1?2:1)+" ms\n";
        text+=!comparison.matchEnabled.load()?"Original levels":!comparison.available.load()?"Match delay limit":comparison.ready.load()?"Match "+juce::String(comparison.matchDb.load(),1)+" dB":"Match: play A and B";
        status.setText(text,juce::dontSendNotification);
    }
    vc::Comparison& comparison;std::function<float()> cpu,latency;std::function<void(int)> selectSlot;
    SnapshotButton buttons[2];juce::TextButton match;juce::Label status;
};
