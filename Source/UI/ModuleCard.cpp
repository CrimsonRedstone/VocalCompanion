#include "UI/ModuleCard.h"
#include "UI/WaveShaperVisualizer.h"
#include "Host/ExternalHost.h"
#include "PluginProcessor.h"

ModuleCard::ModuleCard (vc::VcModule& m, VocalCompanionProcessor& proc,
                        std::function<void()> onDelete, std::function<void()> onBypassChanged, bool expanded)
    : module (m), processor(proc), expandedView(expanded), colours (vc::typeColour (m.getType()))
{
    setWantsKeyboardFocus(true);
    compare=std::make_unique<ComparisonControl>(m.comparison,[&m]{return m.cpuPercent.load();},[&m]{return m.latencyMs.load();},[this,&proc,onBypassChanged](int slot){proc.selectModuleSnapshot(module,slot);bypass.setToggleState(module.isBypassed(),juce::dontSendNotification);if(onBypassChanged)onBypassChanged();repaint();});
    addAndMakeVisible(*compare);
    bypass.setButtonText ("BYP");
    bypass.setClickingTogglesState (true);
    bypass.setToggleState (m.isBypassed(), juce::dontSendNotification);
    bypass.onClick = [this, onBypassChanged] {
        processor.checkpoint();
        module.setBypassed (bypass.getToggleState());
        if (onBypassChanged) onBypassChanged();
        repaint();
    };
    replace.setTooltip ("Replace this module");
    replace.onClick = [this] {
        auto c = replace.getScreenBounds().getCentre();
        if (onReplace) onReplace (c.x, c.y);
    };
    close.onClick = [this,onDelete] {
        juce::Component::SafePointer<ModuleCard> safe(this);
        juce::MessageManager::callAsync ([safe,onDelete] { if (safe && onDelete) onDelete(); });
    };
    addAndMakeVisible (bypass);
    addAndMakeVisible (replace);
    addAndMakeVisible (close);
    if(!expandedView){addAndMakeVisible(duplicate);addAndMakeVisible(full);}
    duplicate.setTooltip("Duplicate this card with independent automation");
    duplicate.onClick=[this]{auto cb=onDuplicate;juce::Component::SafePointer<ModuleCard> safe(this);juce::MessageManager::callAsync([safe,cb]{if(safe&&cb)cb();});};
    full.setTooltip("Full controls: advanced effect settings");
    full.setTitle("Full controls");
    full.onClick=[this]{openFull();};
    if(expandedView){close.setVisible(false);replace.setVisible(false);}


    const auto type = m.getType();
    if (type == vc::ModuleType::Aeterna)
        graph = std::make_unique<AeternaView> (m);
    else if (type == vc::ModuleType::DynamicEq)
        graph = std::make_unique<EqPad> (m, false);
    else if (type == vc::ModuleType::ParaEq)
        graph = std::make_unique<EqPad> (m, true);
    else if (type == vc::ModuleType::WaveShaper)
        graph = std::make_unique<vc::WaveShaperVisualizer> (static_cast<vc::WaveShaperModule&> (m));
    else
    {
        graph = std::make_unique<CurveView> (m);
    }
    addAndMakeVisible (*graph);
    if(auto* eq=dynamic_cast<EqPad*>(graph.get()))
    {
        eq->onGesture=[&proc,&m](int i,bool begin){if(begin)proc.checkpoint();if(auto* p=proc.hostParamFor(&m,i)){if(begin)p->beginChangeGesture();else p->endChangeGesture();}};
        eq->onParamChanged=[&proc,&m](int i){if(auto* p=proc.hostParamFor(&m,i)){const auto& d=m.getParamDescs()[(size_t)i];p->setValueNotifyingHost((m.getParam(i)-d.min)/(d.max-d.min));}};
    }
    if(auto* wave=dynamic_cast<vc::WaveShaperVisualizer*>(graph.get()))
    {
        wave->onBeginEdit=[&proc]{proc.checkpoint();};
        wave->onCurveChanged=[&proc,&m]{if(auto* p=proc.hostParamFor(&m,5))p->setValueNotifyingHost(1.f);};
    }

    const auto& d = m.getParamDescs();
    const int shown = expandedView ? (int)d.size() : m.coreParamCount();
    for (int i = 0; i < shown; ++i)
    {
        if (type == vc::ModuleType::Aeterna && i >= 3 && i < m.coreParamCount())
        {
            auto* c = choices.add (new ParamChoice (m, i, proc.hostParamFor (&m, i)));
            c->onBeginEdit=[&proc]{proc.checkpoint();};
            addAndMakeVisible (c);
            continue;
        }
        auto* k = knobs.add (new ParamKnob (m, i, colours.accent));
        if(i < m.coreParamCount()) ++coreKnobs;
        k->hostParam = proc.hostParamFor (&m, i);
        k->onBeginEdit=[&proc]{proc.checkpoint();};
        addAndMakeVisible (k);
    }

    if(expandedView && shown > m.coreParamCount())
    {
        advancedTitle.setText("ADVANCED",juce::dontSendNotification);
        advancedTitle.setColour(juce::Label::textColourId,colours.accent);
        addAndMakeVisible(advancedTitle);
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
    g.drawText (module.getDisplayName(), 8, 5, getWidth() - (expandedView?50:126), 16, juce::Justification::centredLeft);

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
    const int n = coreKnobs;
    return n <= 3 ? juce::jmax (1, n) : (n == 4 ? 2 : 3);
}

int ModuleCard::preferredHeight() const
{
    const int ch=expandedView?108:module.getType()==vc::ModuleType::Aeterna?92:78;
    const int rows=(coreKnobs+knobColumns()-1)/knobColumns();
    const int advancedRows=(knobs.size()-coreKnobs+2)/3;
    const int graphHeight=expandedView?252:module.getType()==vc::ModuleType::Aeterna?208:88;
    return 26+graphHeight+rows*ch+(choices.size()>0?132:0)
        +(advancedRows>0?24+advancedRows*108:0)+(openGui.isVisible()?28:0)+46;
}

void ModuleCard::resized()
{
    auto r=getLocalBounds().reduced(6);
    auto header=r.removeFromTop(20);
    close.setBounds(header.removeFromRight(20).reduced(1));
    replace.setBounds(header.removeFromRight(20).reduced(1));
    if(!expandedView)
    {
        full.setBounds(header.removeFromRight(20).reduced(1));
        duplicate.setBounds(header.removeFromRight(20).reduced(1));
    }
    bypass.setBounds(header.removeFromRight(34).reduced(1));
    r.removeFromTop(2);
    graph->setBounds(r.removeFromTop(expandedView?252:module.getType()==vc::ModuleType::Aeterna?208:88));
    r.removeFromTop(2);
    if(openGui.isVisible()){openGui.setBounds(r.removeFromTop(24));r.removeFromTop(4);}
    compare->setBounds(r.removeFromBottom(32));
    const int cols=knobColumns(),cw=r.getWidth()/cols;
    const int ch=expandedView?108:module.getType()==vc::ModuleType::Aeterna?92:78;
    for(int i=0;i<coreKnobs;++i)knobs[i]->setBounds(r.getX()+(i%cols)*cw,r.getY()+(i/cols)*ch,cw,ch);
    r.removeFromTop(((coreKnobs+cols-1)/cols)*ch);
    if(choices.size()>=3)
    {
        auto row=r.removeFromTop(44);choices[0]->setBounds(row.removeFromLeft(60));choices[1]->setBounds(row);
        auto second=r.removeFromTop(44);choices[2]->setBounds(choices.size()>3?second.removeFromLeft(76):second);
        if(choices.size()>3)choices[3]->setBounds(second);
        if(choices.size()>4)choices[4]->setBounds(r.removeFromTop(44));
    }
    if(knobs.size()>coreKnobs)
    {
        advancedTitle.setBounds(r.removeFromTop(24));
        const int width=r.getWidth()/3;
        for(int i=coreKnobs;i<knobs.size();++i)
        {const int slot=i-coreKnobs;knobs[i]->setBounds(r.getX()+(slot%3)*width,r.getY()+(slot/3)*108,width,108);}
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

ModuleCard::~ModuleCard() { fullWindow.reset(); }
void ModuleCard::mouseDoubleClick(const juce::MouseEvent&) { if(!expandedView)openFull(); }
void ModuleCard::openFull()
{
    if(auto* ext=dynamic_cast<vc::ExternalModule*>(&module)){ext->openEditor();return;}
    if(fullWindow){fullWindow->setVisible(true);fullWindow->toFront(true);return;}
    struct Window final : juce::DocumentWindow
    {
        Window(juce::String name):DocumentWindow(name,juce::Colour(0xff121720),closeButton){}
        void closeButtonPressed() override {setVisible(false);}
    };
    fullWindow=std::make_unique<Window>(module.getDisplayName()+" — Full controls");
    fullWindow->setLookAndFeel(&getLookAndFeel());
    auto* view=new ModuleCard(module,processor,[]{},[]{},true);
    auto* viewport=new juce::Viewport();viewport->setViewedComponent(view,true);
    view->setSize(680,view->preferredHeight());
    fullWindow->setContentOwned(viewport,false);
    fullWindow->setUsingNativeTitleBar(true);fullWindow->setResizable(true,false);
    fullWindow->centreWithSize(704,juce::jmin(800,view->preferredHeight()+24));
    fullWindow->setVisible(true);
}

bool ModuleCard::keyPressed(const juce::KeyPress& key)
{
    if(key.getModifiers().isCommandDown()||key.getModifiers().isCtrlDown())
    {
        if(key.getKeyCode()=='Z')return key.getModifiers().isShiftDown()?processor.redoEdit():processor.undoEdit();
        if(key.getKeyCode()=='Y')return processor.redoEdit();
    }
    return false;
}
