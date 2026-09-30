#include "UI/Controls.h"
#include "UI/ReactiveDrawing.h"
#include "PluginEditor.h"

ParamKnob::ParamKnob (vc::VcModule& m, int paramIndex, juce::Colour accent)
    : module (m), index (paramIndex), knob ("k", accent)
{
    const auto& d = module.getParamDescs()[(size_t) index];
    knob.setRange ((double) d.min, (double) d.max, d.integer ? 1.0 : 0.0);
    knob.setValue ((double) module.getParam (index), juce::dontSendNotification);
    knob.setDoubleClickReturnValue (true, (double) d.def);
    knob.onValueChange = [this] {
        const float v = (float) knob.getValue();
        module.setParam (index, v);
        if (hostParam != nullptr)
        {
            const auto& d = module.getParamDescs()[(size_t) index];
            const float n01 = (d.max > d.min) ? (v - d.min) / (d.max - d.min) : 0.0f;
            hostParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n01));
        }
        repaint();
    };
    knob.onGesture = [this] (bool begin) {
        if(begin&&onBeginEdit)onBeginEdit();
        if (hostParam == nullptr) return;
        if (begin) hostParam->beginChangeGesture();
        else       hostParam->endChangeGesture();
    };
    knob.onHostMenu = [this] (juce::Point<int> screen) {
        auto* ed = findParentComponentOfClass<VocalCompanionEditor>();
        if (ed == nullptr || hostParam == nullptr) return;
        auto* ctx = ed->getHostContext();
        if (ctx == nullptr) return;
        if (auto menu = ctx->getContextMenuForParameter (hostParam))
        {
            auto popup = menu->getEquivalentPopupMenu();
            if (popup.getNumItems() > 0)
            {
                popup.showMenuAsync (juce::PopupMenu::Options()
                                         .withTargetScreenArea ({ screen.x, screen.y, 2, 2 })
                                         .withParentComponent (ed));
            }
            else
            {
                menu->showNativeMenu (screen);
            }
        }
    };
    name.setText (d.label.toLowerCase(), juce::dontSendNotification);
    if (m.getType() == vc::ModuleType::Aeterna)
        name.setText (d.label.replace (" ", "\n"), juce::dontSendNotification);
    knob.setTooltip (d.label);
    if(m.getType()==vc::ModuleType::PlosiveControl)
    {
        const char* help[]={"Maximum reduction of low-frequency P/B bursts; normal voice passes through", "Highest frequency included in burst removal", "Lower to detect quieter bursts", "Required rise above the recent low-frequency level", "Time for low frequencies to return after a burst", "Processed = normal output. Removed only = monitor the material being removed (often silent)."};
        if(index<(int)std::size(help))knob.setTooltip(help[index]);
    }
    if(m.getType()==vc::ModuleType::AutoTune && index<3)
    {
        const char* help[]={"0 ms snaps immediately after pitch detection; higher values glide toward the target", "Cents of pitch deviation retained around the target; 0 gives full correction", "Slows correction on sustained notes to retain natural movement"};if(index<(int)std::size(help))knob.setTooltip(help[index]);
    }
    name.setJustificationType (juce::Justification::centred);
    name.setColour (juce::Label::textColourId, juce::Colour (0xffd0d4dc));
    name.setFont (juce::Font (juce::FontOptions (12.5f).withStyle ("Bold")));
    name.setInterceptsMouseClicks (false, false);
    value.setJustificationType (juce::Justification::centred);
    value.setColour (juce::Label::textColourId, juce::Colour (0xfff2f4f8));
    value.setFont (juce::Font (juce::FontOptions (13.5f).withStyle ("Bold")));
    // Double-click the value for precise numeric entry; dragging remains available.
    value.setEditable(false,true,false);
    value.onEditorShow=[this]{if(onBeginEdit)onBeginEdit();if(auto* e=value.getCurrentTextEditor())e->setText(juce::String(module.getParam(index),4));};
    value.onTextChange=[this]{knob.setValue(value.getText().getFloatValue());};
    addAndMakeVisible (knob);
    addAndMakeVisible (name);
    addAndMakeVisible (value);
    startTimerHz (24);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    // Pack labels immediately under the dial — don't stretch the cell.
    const int nameH = module.getType() == vc::ModuleType::Aeterna ? 27 : 13, valH = 13;
    const int dial = juce::jmax (28, r.getHeight() - nameH - valH);
    knob.setBounds (r.removeFromTop (dial).reduced (1, 0));
    value.setBounds (r.removeFromTop (valH));
    name.setBounds (r.removeFromTop (nameH));
}

void ParamKnob::paint (juce::Graphics&) {}

void ParamKnob::timerCallback()
{
    const auto& d = module.getParamDescs()[(size_t) index];
    const float v = module.getParam (index);
    if (std::abs (v - (float) knob.getValue()) > 0.0005f)
        knob.setValue ((double) v, juce::dontSendNotification);
    juce::String txt;
    const int iv = (int) std::round (v);
    if (d.labels.size() > 0 && iv >= 0 && iv < d.labels.size())
        txt = d.labels[iv];
    else if (d.integer) txt = juce::String (iv);
    else if (d.suffix.contains ("Hz") && v >= 1000.0f) txt = juce::String (v / 1000.0f, 1) + "k";
    else txt = juce::String (v, v >= 10.0f || v <= -10.0f ? 1 : 2);
    if (d.labels.isEmpty())
        txt += d.suffix;
    if(!value.isBeingEdited())value.setText (txt, juce::dontSendNotification);
}

LevelMeter::LevelMeter (std::function<float()> src, juce::Colour c)
    : source (std::move (src)), colour (c)
{
    startTimerHz (30);
}

void LevelMeter::timerCallback()
{
    shown = vc::onePole (shown, source(), 0.35f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff0a0c10));
    g.fillRoundedRectangle (r, 2.0f);
    const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, shown);
    auto fill = r.removeFromBottom (h);
    g.setGradientFill (juce::ColourGradient (colour.darker (0.4f), fill.getBottomLeft(),
                                             colour, fill.getTopLeft(), false));
    g.fillRoundedRectangle (fill, 2.0f);
}

CurveView::CurveView (vc::VcModule& m) : module (m), visual (m)
{
    startTimerHz (30);
}

void CurveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0x44000000));
    g.fillRoundedRectangle (r, 4.0f);
    paintReactive (g, r, module, visual);
}

EqPad::EqPad (vc::VcModule& m, bool freqMovable)
    : module (m), visual (m), freqMovable (freqMovable)
{
    setTooltip ("Drag numbered bands. Curve includes filters; spectrum is live output. Dynamic EQ curve shows its linear filters; air saturation is visible in the spectrum.");
    startTimerHz (30);
}

juce::Point<float> EqPad::nodePos (int i, juce::Rectangle<float> r) const
{
    const float hzFixed[5] = { 120.0f, 400.0f, 1200.0f, 3500.0f, 12000.0f };
    float hz, g;
    if (freqMovable)
    {
        hz = module.getParam (3 + i);
        g = module.getParam (i);
    }
    else
    {
        hz = i<4 ? module.advanced(3+i) : hzFixed[i];
        g = module.getParam (i);
    }
    const float t = std::log (juce::jmax (20.0f, hz) / 40.0f) / std::log (500.0f);
    const float x = r.getX() + juce::jlimit (0.0f, 1.0f, t) * r.getWidth();
    const float y = r.getCentreY() - (g / 18.0f) * r.getHeight() * 0.42f;
    return { x, y };
}

void EqPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
    g.setColour (juce::Colour (0x44000000));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
    auto ac = vc::typeColour (module.getType()).accent;
    const int N = nodeCount();
    visual.spectra (g, r.reduced (0, 5), ac.withAlpha (.5f));
    vc::Biquad filters[7];
    int count = 0;
    const double sr = visual.frame.sampleRate;
    if (freqMovable)
    {
        filters[count++].setHighpass (module.getParam (6), module.advanced(3), sr);
        filters[count++].setLowpass (std::min (module.getParam (7), (float) sr*.45f), module.advanced(4), sr);
        for (int i=0; i<3; ++i) filters[count++].setPeak (module.getParam (3+i), module.advanced(i), module.getParam(i), sr);
    }
    else
    {
        const float hz[] = {120,400,1200,3500};
        for (int i=0; i<4; ++i) filters[count++].setPeak (module.advanced(3+i),module.getParam(6+i),visual.live ? module.getVisualEqGain(i) : module.getParam(i),sr);
        filters[count++].setHighShelf (6500,module.getParam(4),sr);
        filters[count++].setPeak (11000,.7f,module.getParam(4)*.7f,sr);
    }
    juce::Path p;
    for (int i=0; i<160; ++i)
    {
        const float t=i/159.f, hz=40*std::pow(500.f,t);
        float mag=1;
        for (int f=0; f<count; ++f) mag*=filters[f].magnitude (std::min(hz,(float)sr*.49f),sr);
        const float x=r.getX()+t*r.getWidth(), y=r.getCentreY()-juce::jlimit(-21.f,21.f,vc::gainToDb(mag))/18*r.getHeight()*.42f;
        if (i==0) p.startNewSubPath (x,y); else p.lineTo(x,y);
    }
    g.setColour (ac.withAlpha (0.25f));
    auto fill = p;
    fill.lineTo (r.getRight(), r.getCentreY());
    fill.lineTo (r.getX(), r.getCentreY());
    fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (ac);
    g.strokePath (p, juce::PathStrokeType (1.7f));

    g.setFont (juce::Font (juce::FontOptions (8.5f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff8a90a0));
    auto tick = [&] (float hz, const juce::String& lab)
    {
        const float t = std::log (juce::jmax (20.0f, hz) / 40.0f) / std::log (500.0f);
        const float x = r.getX() + juce::jlimit (0.0f, 1.0f, t) * r.getWidth();
        g.drawText (lab, (int) x - 12, (int) r.getBottom() - 2, 24, 10, juce::Justification::centred);
    };
    tick (100.0f, "100");
    tick (1000.0f, "1K");
    tick (10000.0f, "10K");

    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
    for (int i = 0; i < N; ++i)
    {
        auto pt = nodePos (i, r);
        g.setColour (juce::Colour (0xff2a2c32));
        g.fillEllipse (pt.x - 8.0f, pt.y - 8.0f, 16.0f, 16.0f);
        g.setColour (ac);
        g.drawEllipse (pt.x - 8.0f, pt.y - 8.0f, 16.0f, 16.0f, 1.4f);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String (i + 1), (int) pt.x - 8, (int) pt.y - 8, 16, 16, juce::Justification::centred);
    }
}

void EqPad::mouseDown (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
    float best = 22.0f;
    drag = -1;
    for (int i = 0; i < nodeCount(); ++i)
    {
        auto p = nodePos (i, r);
        const float d = p.getDistanceFrom (e.position);
        if (d < best) { best = d; drag = i; }
    }
    if(drag>=0&&onGesture){onGesture(drag,true);if(freqMovable)onGesture(3+drag,true);}
}

void EqPad::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
    const float g = juce::jlimit (-18.0f, 18.0f,
        (r.getCentreY() - e.position.y) / (r.getHeight() * 0.42f) * 18.0f);
    module.setParam (drag, g);
    if(onParamChanged)onParamChanged(drag);
    if (freqMovable)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
        const float hz = 40.0f * std::pow (500.0f, t);
        module.setParam (3 + drag, hz);
        if(onParamChanged)onParamChanged(3+drag);
    }
    repaint();
}

void EqPad::mouseUp (const juce::MouseEvent&) {if(drag>=0&&onGesture){onGesture(drag,false);if(freqMovable)onGesture(3+drag,false);}drag=-1;}

