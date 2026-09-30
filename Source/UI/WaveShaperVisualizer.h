#pragma once
#include <JuceHeader.h>
#include "../DSP/WaveShaper.h"
namespace vc
{
/** Interactive input/output transfer editor. Points and segment tension are
    module state, so preset, A/B, duplicate and undo all use the same curve. */
class WaveShaperVisualizer : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit WaveShaperVisualizer(WaveShaperModule& m):module(m)
    {setTooltip("Double-click to add a point; drag points or square tension handles; right-click for curve options. Shift = fine movement.");startTimerHz(30);}
    std::function<void()> onBeginEdit,onCurveChanged;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff141a22));auto r=plot();
        g.setColour(juce::Colour(0xff293543));
        for(int i=0;i<=4;++i){g.drawVerticalLine((int)(r.getX()+r.getWidth()*i/4),r.getY(),r.getBottom());g.drawHorizontalLine((int)(r.getY()+r.getHeight()*i/4),r.getX(),r.getRight());}
        g.setColour(juce::Colour(0xff66717f));g.drawLine(r.getX(),r.getBottom(),r.getRight(),r.getY(),.6f);
        juce::Path path;for(int i=0;i<=256;++i){auto p=screen({i/128.f-1,module.evaluateShape(i/128.f-1),0,0});if(i==0)path.startNewSubPath(p);else path.lineTo(p);}
        g.setColour(juce::Colour(0xffffbd70));g.strokePath(path,juce::PathStrokeType(2));
        auto pts=module.getPoints();
        if((int)module.getParam(5)==5)
        for(size_t i=0;i<pts.size();++i)
        {
            auto p=screen(pts[i]);g.setColour(juce::Colours::white);g.fillEllipse(p.x-4,p.y-4,8,8);
            if(i>0){float x=(pts[i-1].x+pts[i].x)*.5f;auto h=screen({x,module.evaluateShape(x),0,0});g.setColour(juce::Colour(0xffc78e56));g.fillRect(h.x-3,h.y-3,6.f,6.f);}
        }
        g.setColour(juce::Colour(0xffadbac9));g.setFont(10.f);g.drawText("INPUT (horizontal)  /  OUTPUT (vertical)",4,getHeight()-13,getWidth()-8,12,juce::Justification::centredLeft);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto pts=module.getPoints();if(pts.size()>=32)return;begin();auto p=point(e.position);
        for(auto v:pts)if(std::abs(v.x-p.x)<.01f)return;
        pts.push_back(p);commit(pts);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        drag=-1;tension=false;auto pts=module.getPoints();
        for(int i=0;i<(int)pts.size();++i)if(screen(pts[(size_t)i]).getDistanceFrom(e.position)<10){drag=i;break;}
        if(e.mods.isPopupMenu()){menu(e,drag);return;}
        if(drag<0)for(int i=1;i<(int)pts.size();++i){float x=(pts[(size_t)i-1].x+pts[(size_t)i].x)*.5f;if(screen({x,module.evaluateShape(x),0,0}).getDistanceFrom(e.position)<10){drag=i;tension=true;break;}}
        if(drag>=0){begin();original=pts[(size_t)drag];}
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto pts=module.getPoints();if(drag<0||drag>=(int)pts.size())return;
        const float fine=e.mods.isShiftDown()?.1f:1;
        auto& p=pts[(size_t)drag];
        if(tension)p.tension=clamp(original.tension-e.getDistanceFromDragStartY()/100.f,-1,1);
        else
        {
            p.x=clamp(original.x+2*e.getDistanceFromDragStartX()/plot().getWidth()*fine,drag==0?-1:pts[(size_t)drag-1].x+.005f,drag==(int)pts.size()-1?1:pts[(size_t)drag+1].x-.005f);
            p.y=clamp(original.y-2*e.getDistanceFromDragStartY()/plot().getHeight()*fine,-1,1);
        }
        commit(pts);
    }
    void mouseUp(const juce::MouseEvent&) override {drag=-1;}
private:
    WaveShaperModule& module;int drag=-1;bool tension=false;ShapePoint original;
    juce::Rectangle<float> plot() const {return getLocalBounds().toFloat().reduced(8).withTrimmedBottom(12);}
    juce::Point<float> screen(ShapePoint p) const {auto r=plot();return {r.getX()+(p.x+1)*.5f*r.getWidth(),r.getBottom()-(p.y+1)*.5f*r.getHeight()};}
    ShapePoint point(juce::Point<float> p) const {auto r=plot();return {clamp(2*(p.x-r.getX())/r.getWidth()-1,-.995f,.995f),clamp(2*(r.getBottom()-p.y)/r.getHeight()-1,-1,1),0,0};}
    void begin(){if(onBeginEdit)onBeginEdit();}
    void commit(const std::vector<ShapePoint>& pts){module.setPoints(pts);if(onCurveChanged)onCurveChanged();repaint();}
    void timerCallback() override {repaint();}
    void menu(const juce::MouseEvent& e,int index)
    {
        juce::PopupMenu m;m.addItem(1,"Add point");m.addItem(2,"Delete point",index>0&&index<(int)module.getPoints().size()-1);
        m.addSeparator();m.addItem(3,"Single curve",index>0);m.addItem(4,"Double curve",index>0);m.addItem(5,"Hold",index>0);
        m.addSeparator();m.addItem(6,"Reset to linear");m.addItem(7,"Flip output");
        auto pos=point(e.position);juce::Component::SafePointer<WaveShaperVisualizer> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,index,pos](int choice){
            if(!safe||choice==0)return;auto pts=safe->module.getPoints();safe->begin();
            if(choice==1&&pts.size()<32){for(auto p:pts)if(std::abs(p.x-pos.x)<.005f)return;pts.push_back(pos);}
            if(choice==2&&index>0&&index<(int)pts.size()-1)pts.erase(pts.begin()+index);
            if(choice>=3&&choice<=5&&index>0&&index<(int)pts.size())pts[(size_t)index].curve=choice-3;
            if(choice==6)pts={{-1,-1,0,0},{0,0,0,0},{1,1,0,0}};
            if(choice==7)for(auto& p:pts)p.y=-p.y;
            safe->commit(pts);
        });
    }
};
}
