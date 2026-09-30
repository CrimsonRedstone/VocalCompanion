#include "AeternaView.h"
#include "AeternaAssets.h"

struct AeternaView::Atlases
{
    std::array<juce::Image, 4> images;
    Atlases()
    {
        const char* data[] { AeternaAssets::aeterna_singing_phase_0_png, AeternaAssets::aeterna_singing_phase_1_png,
                            AeternaAssets::aeterna_singing_phase_2_png, AeternaAssets::aeterna_singing_phase_3_png };
        const int sizes[] { AeternaAssets::aeterna_singing_phase_0_pngSize, AeternaAssets::aeterna_singing_phase_1_pngSize,
                            AeternaAssets::aeterna_singing_phase_2_pngSize, AeternaAssets::aeterna_singing_phase_3_pngSize };
        for (int i = 0; i < 4; ++i) images[(size_t) i] = juce::ImageFileFormat::loadFrom (data[i], (size_t) sizes[i]);
    }
};
AeternaView::AeternaView (vc::VcModule& m) : module (m), velocity (m.getParam (0))
{
    glass = m.getParam (1) / 100; shimmer = m.getParam (2) / 100;
    setInterceptsMouseClicks (false, false);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}
void AeternaView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double delta = juce::jlimit (0.0, 100.0, now - lastTime); lastTime = now;
    const float bpm = module.getMeter (1);
    const double loop = bpm > 0 ? juce::jlimit (600.0, 2400.0, 120000.0 / bpm) : 880.0;
    phase = std::fmod (phase + delta / loop, 1.0);
    velocity += (1 - std::exp (-(float) delta / 55)) * (module.getParam (0) - velocity);
    glass += (1 - std::exp (-(float) delta / 90)) * (module.getParam (1) / 100 - glass);
    shimmer += (1 - std::exp (-(float) delta / 90)) * (module.getParam (2) / 100 - shimmer);
    glow += .3f * (juce::jlimit (0.f, 1.f, module.getMeter() * 4) - glow);
    repaint();
}
void AeternaView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff17131f)); g.fillRoundedRectangle (bounds, 5);
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (getLocalBounds().reduced (2));
    const float strength = module.isBypassed() ? .2f : 1.f;
    const float glassMix = glass * strength, light = shimmer * strength;
    const float cx = bounds.getCentreX(), cy = 91;
    const juce::Colour gold (0xffffdca0);
    const juce::Colour palette[] { juce::Colour (0xff519de0), juce::Colour (0xffcf6bad),
        juce::Colour (0xffe7af65), juce::Colour (0xff65c5b6), juce::Colour (0xff9771e0), juce::Colour (0xffe87387) };
    // Stained-glass rose window and tall lancet panes, drawn behind the original sprite.
    if (glassMix > .001f)
    {
        for (int panel = 0; panel < 5; ++panel)
        {
            const float x = 5 + (float) panel * (bounds.getWidth() - 10) / 5;
            const float w = (bounds.getWidth() - 10) / 5 - 3;
            juce::Path pane; pane.startNewSubPath (x, bounds.getBottom());
            pane.lineTo (x, 58); pane.quadraticTo (x, 32, x + w / 2, 15);
            pane.quadraticTo (x + w, 32, x + w, 58); pane.lineTo (x + w, bounds.getBottom()); pane.closeSubPath();
            g.setGradientFill (juce::ColourGradient (palette[panel].withAlpha (.36f * glassMix), x, 50,
                palette[(panel + 2) % 6].withAlpha (.12f * glassMix), x + w, 208, false));
            g.fillPath (pane); g.setColour (gold.withAlpha (.22f * glassMix)); g.strokePath (pane, juce::PathStrokeType (1.1f));
        }
        for (int ring = 0; ring < 2; ++ring)
            for (int petal = 0; petal < 12; ++petal)
            {
                const float a = (float) petal * vc::kTwoPi / 12 - vc::kPi / 2;
                const float b = a + vc::kTwoPi / 12;
                const float inner = ring == 0 ? 17.f : 44.f, outer = ring == 0 ? 43.f : 72.f;
                juce::Path pane;
                pane.startNewSubPath (cx + std::cos (a) * inner, cy + std::sin (a) * inner);
                pane.lineTo (cx + std::cos (a + .08f) * outer, cy + std::sin (a + .08f) * outer);
                pane.lineTo (cx + std::cos (b - .08f) * outer, cy + std::sin (b - .08f) * outer);
                pane.lineTo (cx + std::cos (b) * inner, cy + std::sin (b) * inner); pane.closeSubPath();
                const auto colour = palette[(petal + ring * 2) % 6];
                g.setGradientFill (juce::ColourGradient (colour.withAlpha (.55f * glassMix), cx, cy,
                    colour.brighter (.5f).withAlpha (.25f * glassMix), cx + std::cos (a) * outer, cy + std::sin (a) * outer, false));
                g.fillPath (pane); g.setColour (juce::Colour (0xff0e101e).withAlpha (.8f * glassMix));
                g.strokePath (pane, juce::PathStrokeType (1.5f));
                g.setColour (gold.withAlpha (.35f * glassMix)); g.strokePath (pane, juce::PathStrokeType (.45f));
            }
    }
    // Shimmer has its own visible light level even in silence; audio adds a gentle pulse.
    if (light > .001f)
    {
        const float pulse = .85f + glow * .15f;
        g.setGradientFill (juce::ColourGradient (gold.withAlpha (.28f * light * pulse), cx, cy,
            gold.withAlpha (0.f), cx + 103, cy, true)); g.fillRect (bounds);
        for (int pass = 6; pass > 0; --pass)
        {
            g.setColour (gold.withAlpha (light * .018f * (float) (7 - pass)));
            g.drawEllipse (cx - 67, 24, 134, 134, (float) pass * 2);
            g.drawEllipse (cx - 73, 18, 146, 146, (float) pass * 1.5f);
        }
        for (int i = 0; i < 12; ++i)
        {
            const float angle = (float) i * vc::kTwoPi / 12;
            const float x = cx + std::cos (angle) * 81, y = cy + std::sin (angle) * 81;
            g.setColour (gold.withAlpha (.7f * light));
            g.drawLine (x - 2, y, x + 2, y, .7f); g.drawLine (x, y - 3, x, y + 3, .7f);
        }
        g.setColour (gold.withAlpha (.28f * light)); g.drawRoundedRectangle (bounds.reduced (4), 5, 2);
    }
    g.setColour (gold.withAlpha (.08f + .72f * light + glow * .12f));
    g.drawEllipse (cx - 67, 24, 134, 134, 1);
    g.drawEllipse (cx - 73, 18, 146, 146, .6f);
    const auto source = frameBounds ((int) std::round (velocity));
    const auto& sprite = atlases->images[(size_t) juce::jlimit (0, 3, (int) (phase * 4))];
    const int x = (getWidth() - 128) / 2, y = getHeight() - 192 - 4;
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    if (light > .001f)
    {
        g.setColour (gold.withAlpha (.055f * light));
        for (int dx : {-2, 0, 2}) for (int dy : {-2, 0, 2})
            if (dx != 0 || dy != 0) g.drawImage (sprite, x + dx, y + dy, 128, 192, source.getX(), source.getY(), 128, 192, true);
    }
    g.setOpacity (module.isBypassed() ? .4f : 1.f);
    g.drawImage (sprite, x, y, 128, 192, source.getX(), source.getY(), 128, 192);
    if (glassMix > .001f)
    {
        juce::ColourGradient reflection (palette[0].withAlpha (.16f * glassMix), (float) x, (float) y,
            palette[1].withAlpha (.12f * glassMix), (float) (x + 128), (float) (y + 192), false);
        reflection.addColour (.42, juce::Colours::white.withAlpha (.25f * glassMix));
        reflection.addColour (.48, juce::Colours::white.withAlpha (0.f));
        reflection.addColour (.7, palette[3].withAlpha (.2f * glassMix));
        g.setGradientFill (reflection);
        g.drawImage (sprite, x, y, 128, 192, source.getX(), source.getY(), 128, 192, true);
    }
    if(module.getParam(6)>.5f)
    {
        const int notes=(int)module.getMeter(4);
        g.setColour(juce::Colour(0xffe5c992));
        g.setFont(juce::Font(juce::FontOptions(10.f)));
        g.drawText(notes>0?"MIDI: "+juce::String(notes)+" notes":"MIDI: waiting for notes",getLocalBounds().removeFromTop(15),juce::Justification::centred);
    }
}
ParamChoice::ParamChoice (vc::VcModule& m, int i, juce::AudioProcessorParameter* p) : module (m), index (i), host (p)
{
    const auto& desc = m.getParamDescs()[(size_t) i];
    label.setText (desc.label, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (10.f)));
    choice.addItemList (desc.labels, 1);
    choice.setSelectedId ((int) m.getParam (i) + 1, juce::dontSendNotification);
    choice.setTooltip (i==6?"Scale: automatic harmony. MIDI notes: route a MIDI Out channel to this plugin and play up to eight notes. The choir still needs vocal audio.":i==7?"Filter incoming MIDI notes by channel; Any accepts channels 1–16.":desc.label);
    choice.onChange = [this] {
        if(onBeginEdit)onBeginEdit();
        const float v = (float) (choice.getSelectedId() - 1);
        module.setParam (index, v);
        if (host)
        {
            const auto& d = module.getParamDescs()[(size_t) index];
            host->beginChangeGesture(); host->setValueNotifyingHost ((v - d.min) / (d.max - d.min)); host->endChangeGesture();
        }
    };
    addAndMakeVisible (label); addAndMakeVisible (choice); startTimerHz (24);
}
void ParamChoice::resized()
{
    auto r = getLocalBounds().reduced (2, 0);
    label.setBounds (r.removeFromTop (15)); choice.setBounds (r.removeFromTop (25));
}
void ParamChoice::timerCallback() { choice.setSelectedId ((int) module.getParam (index) + 1, juce::dontSendNotification); }
