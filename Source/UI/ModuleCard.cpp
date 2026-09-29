#include "UI/ModuleCard.h"
#include "UI/WaveShaperVisualizer.h"
#include "Host/ExternalHost.h"
#include "PluginProcessor.h"

ModuleCard::ModuleCard (vc::VcModule& m, VocalCompanionProcessor& proc,
                        std::function<void()> onDelete, std::function<void()> onBypassChanged)
    : module (m), colours (vc::typeColour (m.getType()))
{
    compare=std::make_unique<ComparisonControl>(m.comparison,[&m]{return m.cpuPercent.load();},[&m]{return m.latencyMs.load();},[this,&proc,onBypassChanged](int slot){proc.selectModuleSnapshot(module,slot);bypass.setToggleState(module.isBypassed(),juce::dontSendNotification);if(onBypassChanged)onBypassChanged();repaint();});
    addAndMakeVisible(*compare);
    bypass.setButtonText ("BYP");
    bypass.setClickingTogglesState (true);
    bypass.setToggleState (m.isBypassed(), juce::dontSendNotification);
    bypass.onClick = [this, onBypassChanged] {
        module.setBypassed (bypass.getToggleState());
        if (onBypassChanged) onBypassChanged();
        repaint();
    };
    replace.setTooltip ("Replace this module");
    replace.onClick = [this] {
        auto c = replace.getScreenBounds().getCentre();
        if (onReplace) onReplace (c.x, c.y);
    };
    close.onClick = [onDelete] {
        juce::MessageManager::callAsync ([onDelete] { if (onDelete) onDelete(); });
    };
    addAndMakeVisible (bypass);
    addAndMakeVisible (replace);
    addAndMakeVisible (close);

    const auto type = m.getType();
    if (type == vc::ModuleType::Aeterna)
        graph = std::make_unique<AeternaView> (m);
    else if (type == vc::ModuleType::DynamicEq)
        graph = std::make_unique<EqPad> (m, false);
    else if (type == vc::ModuleType::ParaEq)
        graph = std::make_unique<EqPad> (m, true);
    else if (type == vc::ModuleType::WaveShaper)
        graph = std::make_unique<vc::WaveShaperVisualizer> (static_cast<vc::WaveShaperModule&> (m).getDsp());
    else
    {
        graph = std::make_unique<CurveView> (m);
    }
    addAndMakeVisible (*graph);

    const auto& d = m.getParamDescs();
    const int shown = (int) d.size();
    for (int i = 0; i < shown; ++i)
    {
        if (type == vc::ModuleType::Aeterna && i >= 3)
        {
            auto* c = choices.add (new ParamChoice (m, i, proc.hostParamFor (&m, i)));
            addAndMakeVisible (c);
            continue;
        }
        auto* k = knobs.add (new ParamKnob (m, i, colours.accent));
        k->hostParam = proc.hostParamFor (&m, i);
        addAndMakeVisible (k);
    }

    if (auto* ext = dynamic_cast<vc::ExternalModule*> (&m))
    {
        openGui.onClick = [ext] { ext->openEditor(); };
        addAndMakeVisible (openGui);
    }
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours.fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours.fillHi.withAlpha (0.55f));
    g.fillRoundedRectangle (r.removeFromTop (28.0f).withTrimmedBottom (-4.0f), 6.0f);

    g.setColour (colours.accent.withAlpha (module.isBypassed() ? 0.3f : 0.9f));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (module.getDisplayName(), 8, 5, getWidth() - 92, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

    if (module.isBypassed())
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }
}

int ModuleCard::knobColumns() const
{
    const int n = knobs.size();
    return n <= 3 ? juce::jmax (1, n) : (n == 4 ? 2 : 3);
}

int ModuleCard::preferredHeight() const
{
    const int rows = (knobs.size() + knobColumns() - 1) / knobColumns();
    return 160 + (module.getType() == vc::ModuleType::Aeterna ? 266 : 0) + (module.getType() == vc::ModuleType::External ? 28 : 0) + rows * 78;
}

void ModuleCard::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto header = r.removeFromTop (20);
    close.setBounds (header.removeFromRight (20).reduced (1));
    replace.setBounds (header.removeFromRight (22).reduced (1));
    bypass.setBounds (header.removeFromRight (34).reduced (1));
    r.removeFromTop (2);
    graph->setBounds (r.removeFromTop (module.getType() == vc::ModuleType::Aeterna ? 208 : 88));
    r.removeFromTop (2);
    if (openGui.isVisible())
    {
        openGui.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
    }
    compare->setBounds(r.removeFromBottom(32));
    const int n = knobs.size();
    if (n == 0) return;
    const int cols = knobColumns();
    const int cw = r.getWidth() / cols;
    const int ch = module.getType() == vc::ModuleType::Aeterna ? 92 : 78; // fixed — labels sit against the dial, cards don't stretch knobs
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
    r.removeFromTop (((n + cols - 1) / cols) * ch);
    if (choices.size() >= 3)
    {
        auto row = r.removeFromTop (44);
        choices[0]->setBounds (row.removeFromLeft (60));
        choices[1]->setBounds (row);
        auto second=r.removeFromTop(44);
        choices[2]->setBounds(choices.size()>3?second.removeFromLeft(76):second);
        if(choices.size()>3)choices[3]->setBounds(second);
        if(choices.size()>4)choices[4]->setBounds(r.removeFromTop(44));
    }
}

void ModuleCard::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    if (onDragStart) onDragStart (this);
}

void ModuleCard::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.getDistanceFromDragStart() > 6)
        if (auto* c = findParentComponentOfClass<juce::DragAndDropContainer>())
            c->startDragging ("module:" + module.instanceId.toString(), this);
}
