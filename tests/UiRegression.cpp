#include "PluginEditor.h"
#include "Presets.h"
#include <iostream>
#include <set>
#include <stdexcept>

static void require (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
}

template <typename T>
static T* findChild (juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* found = dynamic_cast<T*> (child)) return found;
        if (auto* found = findChild<T> (*child)) return found;
    }
    return nullptr;
}

static juce::Button* findButton (juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*> (child))
            if (button->getButtonText() == text) return button;
        if (auto* found = findButton (*child, text)) return found;
    }
    return nullptr;
}

static void click (juce::Component& parent, const juce::String& text)
{
    auto* button = findButton (parent, text);
    require (button != nullptr, "Expected button missing");
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

static void snapshot (juce::Component& c, const juce::File& dir, const char* name)
{
    if (dir == juce::File()) return;
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    dir.createDirectory();
    auto file = dir.getChildFile (name);
    file.deleteFile();
    juce::FileOutputStream out (file);
    require (out.openedOk(), "Cannot write snapshot");
    juce::PNGImageFormat png;
    require (png.writeImageToStream (c.createComponentSnapshot (c.getLocalBounds()), out), "Snapshot failed");
    out.flush();
}

void runAeternaRegression();
void runAudioRepairRegression();
void runRevision17Regression();
void runRevision18Regression();
void runRevision19Regression();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        if(argc<3||juce::String(argv[2])!="--ui-only")
        {
            runRevision19Regression();
            runRevision18Regression();
            runRevision17Regression();
            runAudioRepairRegression();
            runAeternaRegression();
        }
        VocalCompanionProcessor proc;
        for (auto type : vc::kPalette)
        {
            auto module = vc::createModule (type);
            ModuleCard card (*module, proc, [] {}, [] {});
            card.setBounds (0, 0, ModuleCard::preferredWidth, card.preferredHeight());
            int controls = 0;
            for (auto* child : card.getChildren())
                if (auto* knob = dynamic_cast<ParamKnob*> (child))
                {
                    ++controls;
                    require (knob->isVisible(), "Hidden module control");
                    require (card.getLocalBounds().contains (knob->getBounds()), "Clipped module control");
                }
            require (controls == (int) module->getParamDescs().size(), "Missing module parameter");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
        }
        {
            auto module = vc::createModule (vc::ModuleType::WaveShaper);
            module->prepare (48000.0, 128, 2);
            module->setParam (0, 6.0f);
            module->setParam (3, 0.7f);
            module->setParam (4, 1.0f);
            module->setParam (5, 4.0f);
            const float initialCurve = static_cast<vc::WaveShaperModule&> (*module).evaluateShape (0.65f);
            module->setParam (5, 1.0f);
            require (std::abs (static_cast<vc::WaveShaperModule&> (*module).evaluateShape (0.65f) - initialCurve) > 0.01f,
                     "Waveshaper preview did not follow parameter edits");
            module->setParam (5, 4.0f);
            juce::AudioBuffer<float> audio (2, 128);
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    audio.setSample (channel, sample, 0.4f * std::sin (vc::kTwoPi * 220.0f * sample / 48000.0f));
            module->process (audio);
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    require (std::isfinite (audio.getSample (channel, sample)), "Waveshaper produced non-finite audio");
            auto state = module->toValueTree();
            auto restored = vc::createModule (vc::typeFromId (state["type"].toString()));
            restored->fromValueTree (state);
            require (restored->getType() == vc::ModuleType::WaveShaper
                     && restored->getParam (5) == 4.0f && restored->getParam (4) == 1.0f,
                     "Waveshaper state did not round-trip");
        }
        {
            auto m = vc::createModule(vc::ModuleType::Gain);
            auto telemetry = m->visual;
            auto card = std::make_unique<ModuleCard>(*m,proc,[]{},[]{});
            require(telemetry->readers.load()==1,"Graph did not subscribe");
            m.reset(); // Chain removal precedes rack card destruction.
            card.reset();
            require(telemetry->readers.load()==0,"Deleted module graph retained subscription");
        }
        auto catalog = vc::getRackPresetEntries();
        std::set<juce::String> keys;
        int builtIn = 0;
        for (const auto& entry : catalog)
        {
            require (keys.insert (entry.key).second, "Duplicate preset key");
            if (entry.source == vc::RackPresetEntry::User) continue;
            ++builtIn;
            auto tree = vc::getRackPresetTree (entry);
            require (tree.hasType ("STATE") && tree.getChildWithName ("CHAIN").isValid(), "Invalid built-in preset");
        }
        for (const auto& e : catalog)
            if (e.source == vc::RackPresetEntry::Studio)
            {
                auto state = vc::getRackPresetTree (e);
                int parameters = 0;
                vc::Chain audition;
                audition.fromValueTree (state.getChildWithName ("CHAIN"));
                for (auto child : state.getChildWithName ("CHAIN"))
                {
                    auto m = vc::createModule (vc::typeFromId (child["type"].toString()));
                    parameters += (int)m->getParamDescs().size();
                    for (int i=0; i<child.getNumProperties(); ++i)
                    {
                        auto name = child.getPropertyName (i).toString();
                        if (name == "type" || name == "limiterVersion") continue;
                        bool found = false;
                        for (const auto& d : m->getParamDescs()) if (d.id == name)
                        {
                            float value = (float)child[name];
                            require (value >= d.min && value <= d.max, "Preset parameter out of range");
                            require (!d.integer || value == std::round(value), "Non-integral preset choice");
                            found = true;
                        }
                        require (found, "Unknown new preset parameter");
                    }
                }
                require (parameters <= 80, "Preset exceeds automation slots");
                audition.prepare (48000,128,2);
                juce::AudioBuffer<float> audio (2,128);
                for (int block=0;block<120;++block)
                {
                    for(int c=0;c<2;++c)for(int i=0;i<128;++i)
                        audio.setSample(c,i,.12f*std::sin(vc::kTwoPi*220*(block*128+i)/48000.f));
                    audition.process(audio,120);
                    for(int c=0;c<2;++c)for(int i=0;i<128;++i)
                        require(std::isfinite(audio.getSample(c,i)) && std::abs(audio.getSample(c,i))<10,"Unstable new preset");
                }
            }
        require (builtIn == 114, "Missing factory preset");
        proc.skinIndex = 3;
        proc.favorites.add ("style:29");
        proc.loadPresetTree (vc::getRackPresetTree (catalog[1]));
        require (proc.skinIndex == 3 && proc.isFavorite ("style:29"), "Preset erased preferences");
        proc.skinIndex = 0;
        proc.getChain().clear();
        proc.getChain().add (vc::ModuleType::Limiter);
        proc.getChain().add (vc::ModuleType::FetComp);
        proc.getChain().add (vc::ModuleType::ParaEq);
        proc.getChain().add (vc::ModuleType::Delay);
        proc.rebindHostParams();
        proc.currentPresetName = "Control Layout Check";
        VocalCompanionEditor editor (proc);
        editor.setVisible (true);
        const auto output = argc > 1 ? juce::File (argv[1]) : juce::File();
        snapshot (editor, output, "01-all-controls.png");

        auto* panel = findChild<EffectList> (editor);
        require (panel != nullptr, "Missing browser");
        panel->showPresets();
        auto* search = findChild<juce::TextEditor> (*panel);
        auto* list = findChild<juce::ListBox> (*panel);
        require (search && list, "Missing preset search or list");
        require (list->getModel()->getNumRows() >= 114, "Incomplete browser catalog");
        require(findButton(*panel,"All voices")==nullptr && findButton(*panel,"Singers")==nullptr,"Obsolete voice filters remain");
        click(*panel,"Rap"); require(list->getModel()->getNumRows()>=5,"Rap category missing");
        click(*panel,"All sounds");
        snapshot (editor, output, "02-presets.png");
        search->setText ("Natural Pitch Polish");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        require (list->getModel()->getNumRows() >= 1, "Search lost matching preset");
        list->getModel()->returnKeyPressed (0);
        require (proc.currentPresetName == "Natural Pitch Polish", "Wrong preset loaded");
        bool foundTuner = false;
        for (int i = 0; i < proc.getChain().size(); ++i)
            if (proc.getChain().get (i)->getType() == vc::ModuleType::AutoTune)
            {
                foundTuner = true;
                require (proc.getChain().get (i)->getParam (0) == 22.0f, "Wrong tuner preset");
            }
        require (foundTuner, "Preset selection collided with old Lead Polish menu");
        search->clear();
        click (*panel, "Favorites");
        require (list->getModel()->getNumRows() == 1, "Favorites filter failed");
        click (*panel, "All");
        click (*panel, "Clean");
        require (list->getModel()->getNumRows() > 0 && list->getModel()->getNumRows() < 114, "Sound filter failed");
        click (*panel, "All sounds");
        search->setText ("zzzz_no_match");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        require (list->getModel()->getNumRows() == 0, "Empty search failed");
        search->clear();
        click (*panel, "Fun");
        snapshot (editor, output, "03-fun.png");
        auto* tab = findButton (*panel, "Fun");
        auto* random = findButton (*panel, "Random Factory Preset");
        require (tab && random && ! tab->getBounds().intersects (random->getBounds()), "Fun controls overlap tabs");
        for (const auto& skin : vc::kSkins)
            require (findButton (*panel, skin.name) != nullptr || juce::String (skin.name) == "Default", "Missing skin");
        click (*panel, "+ Aeterna");
        auto* aeterna = proc.getChain().get (0);
        require (aeterna && aeterna->getType() == vc::ModuleType::Aeterna, "Fun did not add Aeterna");
        require(findChild<juce::Viewport>(*findChild<RackComponent>(editor))->getViewPositionX()==0,"Aeterna not revealed at rack start");
        for (int i = 0; i < 8; ++i) require (proc.hostParamFor (aeterna, i) != nullptr, "Aeterna automation missing");
        {
            ModuleCard card (*aeterna, proc, []{}, []{});
            card.setLookAndFeel (&editor.getLookAndFeel());
            card.setBounds (0, 0, ModuleCard::preferredWidth, card.preferredHeight());
            int controls = 0;
            for (auto* child : card.getChildren())
                if (dynamic_cast<ParamKnob*> (child) || dynamic_cast<ParamChoice*> (child))
                {
                    ++controls;
                    require (child->isVisible() && card.getLocalBounds().contains (child->getBounds()), "Aeterna controls clipped");
                }
            require (controls == 8, "Aeterna controls missing");
            snapshot (card, output, "06-aeterna-card.png");
            aeterna->setParam (0, 127); aeterna->setParam (3, 10); aeterna->setParam (4, 2);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
            snapshot (card, output, "08-aeterna-full.png");
            aeterna->setParam (1, 0); aeterna->setParam (2, 0);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
            snapshot (card, output, "09-aeterna-unlit.png");
            aeterna->setParam (1, 100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
            snapshot (card, output, "10-aeterna-glass.png");
            aeterna->setParam (1, 0); aeterna->setParam (2, 100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
            snapshot (card, output, "11-aeterna-glow.png");
            aeterna->setParam (1, 100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
            snapshot (card, output, "12-aeterna-glass-glow.png");
            card.setLookAndFeel (nullptr);
        }
        for (int v = 0; v < 128; ++v)
            require (juce::Rectangle<int> (0,0,2048,1536).contains (AeternaView::frameBounds (v)), "Aeterna frame out of atlas");
        for (auto* child : panel->getChildren())
            if (child->isVisible()) require (panel->getLocalBounds().contains (child->getBounds()), "Fun control clipped");
        snapshot (editor, output, "07-aeterna-fun.png");
        click (*panel, "Presets");
        list->scrollToEnsureRowIsOnscreen (list->getModel()->getNumRows() - 1);
        snapshot (editor, output, "04-presets-bottom.png");
        editor.setSize (1920, 780);
        snapshot (editor, output, "05-scaled.png");

        for (auto dimensions : { juce::Point<int>{2048,635}, juce::Point<int>{1280,900}, juce::Point<int>{2560,1040} })
        {
            editor.setSize(dimensions.x,dimensions.y);
            auto* root = editor.getChildComponent(0);
            auto screen = root->getBounds().toFloat().transformedBy(root->getTransform());
            require(screen.getX()==0 && screen.getY()==0 && screen.getWidth()>=editor.getWidth() && screen.getHeight()>=editor.getHeight(),"Resize letterboxed content");
        }
        editor.setSize(2048,635);
        snapshot(editor,output,"13-wide-host.png");
        click(*panel,"Effects");
        auto id = proc.getChain().get(0)->instanceId;
        juce::SparseSet<int> selected; selected.addRange({0,1});
        auto description = list->getModel()->getDragSourceDescription(selected);
        require(description.toString()=="module:"+id.toString(),"Effect row drag identity lost");
        const int last = proc.getChain().size()-1;
        panel->itemDropped({description,list,{list->getX()+20,list->getY()+(last+1)*32}});
        require(proc.getChain().get(last)->instanceId==id,"Effect row downward reorder failed");
        panel->itemDropped({description,list,{list->getX()+20,list->getY()}});
        require(proc.getChain().get(0)->instanceId==id,"Effect row upward reorder failed");
        require(proc.hostParamFor(proc.getChain().get(0),0)!=nullptr,"Reorder lost automation binding");

        const int originalCount = proc.getChain().size();
        for(int i=0;i<20;++i)proc.getChain().add(vc::ModuleType::Gain);
        panel->refresh();
        juce::DragAndDropTarget::SourceDetails edge(description,list,{list->getX()+20,list->getBottom()-2});
        panel->itemDragMove(edge);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        require(list->getViewport()->getViewPositionY()>0,"Effect drag did not scroll at edge");
        panel->itemDragExit(edge);
        for(int i=proc.getChain().size()-1;i>=originalCount;--i)proc.getChain().remove(i);
        panel->refresh();

        // Capturing audio must not alter DSP output, even with huge/irregular blocks.
        vc::Chain dryTap, liveTap;
        dryTap.add(vc::ModuleType::Gain); liveTap.add(vc::ModuleType::Gain);
        dryTap.prepare(48000,4096,2); liveTap.prepare(48000,4096,2);
        AudioVisuals analysis(*liveTap.get(0));
        for (int n : {1,63,257,2048,4096})
        {
            juce::AudioBuffer<float> a(2,n),b(2,n);
            for(int c=0;c<2;++c)for(int i=0;i<n;++i)a.setSample(c,i,.2f*std::sin(vc::kTwoPi*220*i/48000.f)*(c?-1.f:1.f));
            b.makeCopyOf(a);dryTap.process(a,120);liveTap.process(b,120);
            for(int c=0;c<2;++c)for(int i=0;i<n;++i)require(a.getSample(c,i)==b.getSample(c,i),"Visual tap changed audio");
            analysis.update();
        }
        require(analysis.energy>0 && analysis.spectrum[1][24]>0,"Live analysis missed anti-phase audio");
        analysis.update(); require(analysis.live,"Visuals blinked between host blocks");
        for(int i=0;i<70;++i)analysis.update();
        require(analysis.energy<.0001f && analysis.output[95]==0,"Stopped playback left live energy stuck");

        // A gallery of real native graph components driven by test audio.
        juce::Image gallery(juce::Image::RGB, 4*240, 6*118, true);
        juce::Graphics gg(gallery); gg.fillAll(juce::Colour(0xff10151c));
        int visualIndex=0;
        for(auto type:vc::kPalette)
        {
            auto m=vc::createModule(type);m->prepare(48000,256,2);m->visual->prepare(48000);
            ModuleCard card(*m,proc,[]{},[]{});card.setBounds(0,0,224,card.preferredHeight());card.setVisible(true);
            auto* graph=card.getChildComponent(3);
            for(auto* c:card.getChildren())if(dynamic_cast<CurveView*>(c)||dynamic_cast<EqPad*>(c))graph=c;
            juce::AudioBuffer<float> audio(2,256);
            for(int tick=0;tick<12;++tick)
            {
                for(int block=0;block<6;++block)
                {
                    for(int c=0;c<2;++c)for(int i=0;i<256;++i)
                        audio.setSample(c,i,.2f*(.4f+.6f*std::sin(tick*.3f))*std::sin(vc::kTwoPi*(220+tick*3)*((tick*6+block)*256+i)/48000.f+c*.3f));
                    m->visual->begin(audio);m->process(audio);m->visual->end(audio);
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
            }
            int x=visualIndex%4*240+8,y=visualIndex/4*118+6;
            gg.setColour(juce::Colours::white);gg.setFont(12.f);gg.drawText(m->getDisplayName(),x,y,224,18,juce::Justification::centredLeft);
            gg.drawImageAt(graph->createComponentSnapshot(graph->getLocalBounds()),x,y+20);
            ++visualIndex;
        }
        if(output!=juce::File()) { juce::FileOutputStream file(output.getChildFile("14-live-graphs.png"));juce::PNGImageFormat().writeImageToStream(gallery,file); }
        {
            VocalCompanionProcessor settings;settings.getChain().clear();settings.getChain().add(vc::ModuleType::Gain);settings.prepareToPlay(48000,256);
            auto* gain=settings.getChain().get(0);const float initial=gain->getParam(1);
            ModuleCard card(*gain,settings,[]{},[]{});card.setBounds(0,0,ModuleCard::preferredWidth,card.preferredHeight());
            gain->setParam(1,2);gain->setParam(0,7);click(card,"B");
            juce::MessageManager::getInstance()->runDispatchLoopUntil(70);
            int index=0;for(auto* c:card.getChildren())if(auto* k=dynamic_cast<ParamKnob*>(c))
            {auto* dial=findChild<juce::Slider>(*k);require(dial!=nullptr,"Snapshot knob missing");if(index==0)require(std::abs(dial->getValue())<.001,"B gain knob did not move");if(index==1)require(std::abs(dial->getValue()-initial)<.001,"B mode knob did not move");++index;}
            snapshot(card,output,"16-gain-snapshot-b.png");click(card,"A");
            juce::MessageManager::getInstance()->runDispatchLoopUntil(70);
            index=0;for(auto* c:card.getChildren())if(auto* k=dynamic_cast<ParamKnob*>(c))
            {auto* dial=findChild<juce::Slider>(*k);if(index==0)require(std::abs(dial->getValue()-7)<.001,"A gain knob did not return");if(index==1)require(std::abs(dial->getValue()-2)<.001,"A mode knob did not return");++index;}
            require(findButton(card,"Off")==nullptr,"Off button remains in A/B");snapshot(card,output,"17-gain-snapshot-a.png");
        }
        {
            VocalCompanionProcessor cleanup;
            cleanup.getChain().clear();
            for(auto type:{vc::ModuleType::BreathControl,vc::ModuleType::VocalRider,vc::ModuleType::PlosiveControl,vc::ModuleType::Aeterna})cleanup.getChain().add(type);
            auto* choir=cleanup.getChain().get(3);choir->setParam(6,1);choir->setParam(0,80);
            cleanup.currentPresetName="Cleanup and MIDI choir";cleanup.initialiseRackSnapshots();cleanup.prepareToPlay(48000,256);
            VocalCompanionEditor ui(cleanup);ui.setSize(1280,700);
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer notes;
            notes.addEvent(juce::MidiMessage::noteOn(1,64,.8f),0);notes.addEvent(juce::MidiMessage::noteOn(1,67,.8f),0);notes.addEvent(juce::MidiMessage::noteOn(1,71,.8f),0);
            for(int block=0;block<300;++block)
            {
                for(int i=0;i<256;++i)for(int c=0;c<2;++c)audio.setSample(c,i,.05f*std::sin(vc::kTwoPi*220*(block*256+i)/48000.f));
                cleanup.processBlock(audio,notes);
                if(block%20==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
            }
            auto* card=findChild<ModuleCard>(ui);require(card!=nullptr,"Cleanup card missing");
            click(*card,"A");require(card->module.comparison.mode.load()==0,"Card A button failed");
            click(*card,"B");require(card->module.comparison.mode.load()==1,"Card B button failed");click(*card,"A");require(findButton(*card,"Off")==nullptr,"Redundant Off button remains");
            auto* top=findChild<TopBar>(ui);require(top!=nullptr,"Top bar missing");
            click(*top,"A");require(cleanup.rackComparison.mode.load()==0,"Rack A button failed");click(*top,"B");require(cleanup.rackComparison.mode.load()==1,"Rack B button failed");click(*top,"A");
            snapshot(ui,output,"15-cleanup-midi-comparison.png");
        }
        vc::Chain chain;
        chain.add (vc::ModuleType::Gain); chain.add (vc::ModuleType::Delay); chain.add (vc::ModuleType::Reverb);
        chain.move (0, 3);
        require (chain.get (2)->getType() == vc::ModuleType::Gain, "Cannot move card to final insertion slot");
        std::cout << "PASS: all module controls, preset catalog, preference preservation, search, selection, filters, tab layout, scaling snapshots and final-slot reorder.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
