#pragma once
#include "AudioVisuals.h"
#include "DSP/Modulation.h"

inline void paintReactive (juce::Graphics& g, juce::Rectangle<float> bounds, vc::VcModule& m, AudioVisuals& v)
{
    using T = vc::ModuleType;
    const auto type = m.getType();
    const auto ac = vc::typeColour (type).accent;
    auto r = bounds.reduced (7, 5);
    auto label = r.removeFromBottom (11);
    r.removeFromBottom (3);
    g.setColour (ac.withAlpha (.08f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int)(r.getY()+r.getHeight()*i/4),r.getX(),r.getRight());
    for (int i = 1; i < 5; ++i) g.drawVerticalLine ((int)(r.getX()+r.getWidth()*i/5),r.getY(),r.getBottom());
    juce::String caption;
    if(type==T::BreathControl||type==T::VocalRider||type==T::PlosiveControl)
    {
        v.history(g,r,ac);
        if(type==T::VocalRider)
        {
            float y=r.getCentreY()-m.getMeter()/24*r.getHeight()*.5f;
            g.setColour(ac);g.drawHorizontalLine((int)y,r.getX(),r.getRight());
            caption="RIDE "+juce::String(m.getMeter(),1)+" dB | TARGET "+juce::String(m.getParam(0),0);
        }
        else
        {
            AudioVisuals::path(g,r.withHeight(r.getHeight()*.4f),v.reduction,juce::Colour(0xffffd887),false);
            caption=type==T::BreathControl?"BREATH / CLICK REPAIR | IN / OUT":"P/B REDUCTION "+juce::String(-vc::gainToDb(1-m.getMeter()),1)+" dB";
            if(m.getParam(5)>.5f)caption="LISTENING TO REMOVED AUDIO";
        }
    }
    else if (type == T::FetComp || type == T::OptoComp)
    {
        v.history (g,r,ac);
        AudioVisuals::path (g,r.withHeight (r.getHeight()*.4f),v.reduction,juce::Colour (0xffffd887),false);
        const float ratio=type==T::FetComp?m.getParam(3):m.getParam(1);
        const float attack=type==T::FetComp?m.getParam(4):m.getParam(2),release=type==T::FetComp?m.getParam(5):m.getParam(3);
        caption = "A "+juce::String(attack,1)+"  R "+juce::String(release,0)+" ms | "+juce::String(ratio,1)+":1";
        const float threshold = type == T::FetComp ? m.getParam (2) : m.getParam (0);
        const float y = r.getBottom()-r.getHeight()*AudioVisuals::level (vc::dbToGain (threshold));
        g.setColour (ac.withAlpha (.45f)); g.drawHorizontalLine ((int)y,r.getX(),r.getRight());
        juce::Path transfer;
        for(int i=0;i<64;++i)
        {
            float db=-60+i/63.f*60;
            float result=db>threshold?threshold+(db-threshold)/ratio:db;
            result+=type==T::OptoComp?m.getParam(4):m.getParam(1);
            float px=r.getX()+i/63.f*r.getWidth(),py=r.getBottom()-AudioVisuals::level(vc::dbToGain(result))*r.getHeight();
            if(i==0)transfer.startNewSubPath(px,py);else transfer.lineTo(px,py);
        }
        g.setColour(ac.withAlpha(.2f));g.strokePath(transfer,juce::PathStrokeType(1.f));
    }
    else if(type == T::Limiter)
    {
        auto yAt=[&](float db){return r.getBottom()-juce::jlimit(0.f,1.f,(db+60)/72)*r.getHeight();};
        const juce::Colour inColour(0xffc79aed),outColour(0xff79e6ae),grColour(0xfff5e9cb);
        std::array<float,96> in{},out{},gr{};
        for(int i=0;i<96;++i)
        {
            in[(size_t)i]=juce::jlimit(0.f,1.f,(vc::gainToDb(v.peakInput[(size_t)i])+m.getParam(2)+60)/72);
            out[(size_t)i]=juce::jlimit(0.f,1.f,(vc::gainToDb(v.peakOutput[(size_t)i])+60)/72);
            gr[(size_t)i]=juce::jlimit(0.f,1.f,1+vc::gainToDb(1-v.reduction[(size_t)i])/36);
        }
        AudioVisuals::path(g,r,in,inColour.withAlpha(.55f),false);
        AudioVisuals::path(g,r,out,outColour,true);
        AudioVisuals::path(g,r,gr,grColour.withAlpha(.8f),false);
        auto marker=[&](float db,juce::Colour c,const char* name,int column){
            float y=yAt(db);g.setColour(c.withAlpha(.7f));g.drawHorizontalLine((int)y,r.getX(),r.getRight());
            g.setFont(juce::Font(juce::FontOptions(8.f)));g.drawText(name,juce::Rectangle<float>(r.getX()+column*r.getWidth()/3,juce::jlimit(r.getY(),r.getBottom()-10,y+1),r.getWidth()/3,10),juce::Justification::centred);
        };
        marker(m.getParam(0),outColour,"CEIL",2);marker(m.getParam(2),inColour,"GAIN",0);
        if(m.getParam(6)<m.getParam(0))marker(m.getParam(6),juce::Colour(0xffee8a7d),"SAT",1);
        // Envelope preview changes immediately with timing controls, even idle.
        juce::Path preview;
        for(int i=0;i<96;++i)
        {
            float ms=i/95.f*std::max(100.f,m.getParam(3)+m.getParam(4)+m.getParam(1)*3);
            const float attack=m.getParam(3),hold=m.getParam(4),release=m.getParam(1)*(.4f+m.getParam(5)*.15f);
            float gain=ms<attack?1-.6f*(1-std::exp(-ms/std::max(.01f,attack/(9-m.getParam(5)))))
                :ms<attack+hold?.4f:1-.6f*std::exp(-(ms-attack-hold)/release);
            float x=r.getX()+i/95.f*r.getWidth(),y=r.getY()+(1-gain)*r.getHeight()*.55f;
            if(i==0)preview.startNewSubPath(x,y);else preview.lineTo(x,y);
        }
        g.setColour(grColour.withAlpha(.23f));g.strokePath(preview,juce::PathStrokeType(1));
        caption="PEAK IN / OUT | GR | ENVELOPE PREVIEW";
    }
    else if (type == T::DeEsser || type == T::AirBreath || type == T::Exciter || type == T::RingMod)
    {
        v.spectra (g,r,ac);
        if (type == T::DeEsser)
        {
            const float lo[] {2500,4000},hi[] {4000,8000};
            for(int i=0;i<2;++i)
            {
                auto xAt=[&](float hz){return r.getX()+r.getWidth()*std::log(hz/40)/std::log(500.f);};
                const float x=xAt(lo[i]),width=xAt(hi[i])-x;
                const float y=r.getBottom()-AudioVisuals::level(vc::dbToGain(m.getParam(i)))*r.getHeight();
                const float range=std::min(r.getBottom()-y,m.getParam(2)/72*r.getHeight());
                const auto colour=i?juce::Colour(0xffd5a3ff):ac;
                g.setColour(colour.withAlpha(.06f+.12f*m.getParam(3)));
                g.fillRect(x,r.getY(),width,r.getHeight());
                g.setColour(colour.withAlpha(.2f));g.fillRect(x,y,width,range);
                g.setColour(colour);g.drawLine(x,y,x+width,y,1.5f);
                const float grDb=v.live?-vc::gainToDb(1-m.getMeter(i+1)):0;
                g.fillRect(x+width-3,y,3.f,std::min(range,grDb/72*r.getHeight()));
            }
            caption="3k / 6k THRESH | " + juce::String(m.getParam(2),0)+" dB | SENS "+juce::String((int)(m.getParam(3)*100))+"%";
        }
        else if (type == T::AirBreath) caption = "AIR  |  " + juce::String ((int)(m.getParam(0)*100)) + "%  |  LIVE SPECTRUM";
        else if (type == T::Exciter) caption = "HARMONICS / WARMTH  |  SPECTRUM";
        else caption = "RING  " + juce::String ((int)m.getParam(0)) + " Hz  |  SIDEBANDS";
    }
    else if (type == T::Gain)
    {
        v.scope(g,r,ac);
        caption="GHOST INPUT / OUTPUT | " + juce::String(m.getParam(0),1)+" dB";
    }
    else if (type == T::Imager || type == T::AutoPan)
    {
        auto c=r.getCentre(); const float radius=r.getHeight()*.46f;
        if(type==T::Imager)
        {
            g.setColour(ac.withAlpha(.16f));g.drawEllipse(c.x-radius,c.y-radius,radius*2,radius*2,1);
            g.drawLine(c.x-radius,c.y-radius,c.x+radius,c.y+radius);
            g.drawLine(c.x-radius,c.y+radius,c.x+radius,c.y-radius);
            float peak=.02f,cross=0,ll=0,rr=0;
            for(int i=0;i<1024;++i)
            {
                for(int side=0;side<2;++side)
                {
                    float l=v.frame.wave[(size_t)(side*2)][(size_t)i],right=v.frame.wave[(size_t)(side*2+1)][(size_t)i];
                    peak=std::max(peak,std::sqrt(l*l+right*right));
                }
                float l=v.frame.wave[2][(size_t)i],right=v.frame.wave[3][(size_t)i];
                cross+=l*right;ll+=l*l;rr+=right*right;
            }
            for(int side=0;side<2;++side)
            {
                juce::Path trace;
                for(int i=0;i<1024;i+=2)
                {
                    const float l=v.live?v.frame.wave[(size_t)(side*2)][(size_t)i]:0,right=v.live?v.frame.wave[(size_t)(side*2+1)][(size_t)i]:0;
                    const float x=c.x+(l-right)*.7071f/peak*radius,y=c.y-(l+right)*.7071f/peak*radius;
                    if(i==0)trace.startNewSubPath(x,y);else trace.lineTo(x,y);
                }
                g.setColour(side?ac.withAlpha(.85f):juce::Colours::white.withAlpha(.25f));
                g.strokePath(trace,juce::PathStrokeType(side?1.f:.6f));
            }
            const float corr=ll*rr>1.e-12f?juce::jlimit(-1.f,1.f,cross/std::sqrt(ll*rr)):0;
            caption="IN / OUT FIELD | CORR " + (v.live&&ll*rr>1.e-12f?juce::String(corr,2):juce::String("--"));
            g.setColour(ac.withAlpha(.6f));g.setFont(juce::Font(juce::FontOptions(8.f)));
            g.drawText("SIDE",r.withWidth(35),juce::Justification::centredLeft);
            g.drawText("MONO",r.withHeight(10),juce::Justification::centred);
        }
        else
        {
            const float pan=std::sin(vc::kTwoPi*m.getVisualPhase())*m.getParam(1);
            const float x=c.x+pan*r.getWidth()*.45f;
            g.setColour(ac.withAlpha(.15f+v.energy*.65f));g.fillEllipse(x-7,c.y-7,14,14);
            g.setColour(ac);g.drawLine(r.getX(),c.y,r.getRight(),c.y,1);
            g.drawVerticalLine((int)c.x,c.y-8,c.y+8);
            g.setFont(juce::Font(juce::FontOptions(8.1f)));
            g.drawText("L",label,juce::Justification::centredLeft);
            g.drawText("R",label,juce::Justification::centredRight);
            g.drawText("PAN POSITION",label,juce::Justification::centred);
            return;
        }
    }
    else if (type == T::Phaser)
    {
        v.spectra(g,r,ac.withAlpha(.55f));
        const double sr=v.frame.sampleRate;
        const float hz=m.getParam(2)*std::pow(2.f,std::sin(vc::kTwoPi*m.getVisualPhase())*m.getParam(1));
        const double wc=std::tan(vc::kPi*juce::jlimit(40.f,(float)sr*.45f,hz)/sr), a=(1-wc)/(1+wc);
        juce::Path p;
        for(int i=0;i<128;++i)
        {
            const float t=i/127.f, f=40*std::pow(500.f,t);
            const auto z=std::polar(1.0,-2*juce::MathConstants<double>::pi*f/sr);
            const auto ap=std::pow((a+z)/(1.0+a*z),(int)m.getParam(3));
            const double mix=m.getParam(5)*.5;
            const auto response=1.0-mix+mix*ap/(1.0-(double)m.getParam(4)*z*ap);
            const float db=juce::jlimit(-36.f,12.f,vc::gainToDb((float)std::abs(response)));
            const float x=r.getX()+t*r.getWidth(), y=r.getBottom()-(db+36)/48*r.getHeight();
            if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);
        }
        g.setColour(ac.withAlpha(.16f));g.strokePath(p,juce::PathStrokeType(5));
        g.setColour(ac);g.strokePath(p,juce::PathStrokeType(1.4f));
        caption="SWEEP RESPONSE  |  LIVE SPECTRUM";
    }
    else if (type == T::Chorus || type == T::Tremolo)
    {
        v.history(g,r,ac.withAlpha(.35f));
        const int voices=type==T::Chorus?(int)m.getParam(3):1;
        const float depth=type==T::Chorus?m.getParam(1)/8:m.getParam(1);
        for (int voice=0;voice<voices;++voice)
        {
            juce::Path p;
            for(int i=0;i<96;++i)
            {
                float x=i/95.f;
                const float ph=std::fmod(x+m.getVisualPhase()+voice/(float)voices,1.f);
                const float wave=vc::lfoValue(ph,type==T::Tremolo?(int)m.getParam(2):0);
                const float y=type==T::Tremolo ? r.getBottom()-(1-depth*(.5f-.5f*wave))*r.getHeight() : r.getCentreY()-wave*(.1f+.35f*depth)*r.getHeight();
                if(i==0)p.startNewSubPath(r.getX()+x*r.getWidth(),y);else p.lineTo(r.getX()+x*r.getWidth(),y);
            }
            g.setColour(ac.withAlpha(.25f+v.energy*.65f));g.strokePath(p,juce::PathStrokeType(1.3f));
        }
        caption=type==T::Chorus?"VOICE MOTION  |  LIVE ENVELOPE":"GAIN MODULATION  |  LIVE ENVELOPE";
    }
    else if (type == T::Delay)
    {
        const float beats[] = {1,.5f,1.f/3,.25f,2,4};
        const float seconds = std::min (2.5f, m.getParam(3)>.5f ? beats[juce::jlimit(0,5,(int)std::round(m.getParam(4)))]*60/std::max(20.f,m.visual->bpm.load()) : m.getParam(0)*.001f);
        for (int tap=1;tap<=6;++tap)
        {
            const float x=r.getX()+std::min(.97f,seconds*tap/std::max(4.f,seconds*6))*r.getWidth();
            const float h=r.getHeight()*std::pow(m.getParam(1),tap-1)*m.getParam(2);
            g.setColour(ac.withAlpha(.25f+v.energy*.7f));
            g.fillRoundedRectangle(x-2,r.getBottom()-h,4,h,2);
        }
        v.history(g,r,ac.withAlpha(.5f));caption="ECHO SPACING  " + juce::String((int)(seconds*1000))+" ms  |  LIVE TAIL";
    }
    else if (type == T::Reverb)
    {
        v.history(g,r,ac.withAlpha(.5f));
        for(int i=0;i<28;++i)
        {
            const float x=i/27.f;
            const float h=std::exp(-x*(5-4*m.getParam(0)))*r.getHeight()*(.25f+.65f*v.energy);
            g.setColour(ac.withAlpha(.18f+(1-x)*.4f));
            g.drawLine(r.getX()+x*r.getWidth(),r.getBottom(),r.getX()+x*r.getWidth(),r.getBottom()-h,1.4f);
        }
        caption="DECAY SKETCH  |  LIVE TAIL ENVELOPE";
    }
    else if (type == T::PitchFormant || type == T::AutoTune)
    {
        v.scope(g,r,ac);
        caption="GHOST INPUT / OUTPUT WAVE | SAME SCALE";
    }
    else if (type == T::MsEq)
    {
        v.spectra(g,r,ac.withAlpha(.5f));
        for(int channel=0;channel<2;++channel)
        {
            vc::Biquad filters[3];
            filters[0].setLowShelf(180,m.getParam(channel*3),v.frame.sampleRate);
            filters[1].setPeak(channel?1800:1200,.9f,m.getParam(channel*3+1),v.frame.sampleRate);
            filters[2].setHighShelf(channel?7000:6500,m.getParam(channel*3+2),v.frame.sampleRate);
            juce::Path p;
            for(int i=0;i<96;++i)
            {
                float f=40*std::pow(500.f,i/95.f), mag=1;for(auto& filter:filters)mag*=filter.magnitude(f,v.frame.sampleRate);
                float x=r.getX()+i/95.f*r.getWidth(),y=r.getCentreY()-juce::jlimit(-18.f,18.f,vc::gainToDb(mag))/36*r.getHeight();
                if(!i)p.startNewSubPath(x,y);else p.lineTo(x,y);
            }
            g.setColour(channel?juce::Colour(0xffffbe81):ac);g.strokePath(p,juce::PathStrokeType(1.5f));
        }
        caption="MID / SIDE EQ  |  LIVE SPECTRUM";
    }
    else { v.scope(g,r,ac);caption=type==T::Bitcrush?"IN / QUANTIZED OUTPUT WAVE":"LIVE OUTPUT WAVE"; }
    g.setColour(ac.withAlpha(.75f));g.setFont(juce::Font(juce::FontOptions(8.1f)));
    g.drawText(caption,label,juce::Justification::centredLeft,true);
}
