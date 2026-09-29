#include "UI/TopBar.h"
#include "Presets.h"

TopBar::TopBar (VocalCompanionProcessor& p, std::function<void()> onChainChanged)
    : proc (p),
      inMeter ([&p] { return p.inputPeak.load(); }, juce::Colour (0xff3ec8e0)),
      outMeter ([&p] { return p.outputPeak.load(); }, juce::Colour (0xffff6a82)),
      chainChanged (std::move (onChainChanged))
{
    compare=std::make_unique<ComparisonControl>(p.rackComparison,std::function<float()>{},[&p]{return p.rackLatencyMs.load();},[this](int slot){proc.selectRackSnapshot(slot);if(chainChanged)chainChanged();rebuildPresetList();});
    addAndMakeVisible(*compare);
    addAndMakeVisible (presetButton);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (inGain);
    addAndMakeVisible (outGain);
    addAndMakeVisible (support);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    inGain.setSliderStyle (juce::Slider::LinearHorizontal);
    outGain.setSliderStyle (juce::Slider::LinearHorizontal);
    inGain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 16);
    outGain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 16);
    inGain.setRange (-24.0, 24.0, 0.1);
    outGain.setRange (-24.0, 24.0, 0.1);
    inGain.setValue (proc.masterInDb.load());
    outGain.setValue (proc.masterOutDb.load());
    inGain.setTextValueSuffix (" dB");
    outGain.setTextValueSuffix (" dB");
    inGain.onValueChange = [this] { proc.masterInDb.store ((float) inGain.getValue()); };
    outGain.onValueChange = [this] { proc.masterOutDb.store ((float) outGain.getValue()); };

    support.setFont (juce::Font (juce::FontOptions (12.0f)), false);
    support.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff8ad4e0));
    support.setTooltip ("This freeware was made by Crimson Redstone. Consider supporting me by purchasing my music on Bandcamp.");

    presetButton.setTooltip ("Open the preset browser");
    presetButton.onClick = [this] { if (onBrowsePresets) onBrowsePresets(); };
    saveBtn.onClick = [this] {
        auto name = juce::File::createLegalFileName (proc.currentPresetName);
        if (name.isEmpty()) name = "My Rack";
        auto chooser = std::make_shared<juce::FileChooser> (
            "Save rack as", vc::getUserPresetDir().getChildFile (name + ".vcpreset"), "*.vcpreset");
        juce::Component::SafePointer<TopBar> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe, chooser] (const juce::FileChooser& fc) {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                const auto file = fc.getResult().withFileExtension ("vcpreset");
                auto state = safe->proc.saveStateTree();
                const auto presetName = file.getFileNameWithoutExtension();
                state.setProperty ("name", presetName, nullptr);
                auto xml = state.createXml();
                if (xml == nullptr || ! xml->writeTo (file))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                        "Preset not saved", "The preset file could not be written.");
                    return;
                }
                safe->proc.currentPresetName = presetName;
                if (safe->chainChanged) safe->chainChanged();
            });
    };
    loadBtn.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Load preset", vc::getUserPresetDir(), "*.vcpreset");
        juce::Component::SafePointer<TopBar> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, chooser] (const juce::FileChooser& fc) {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                const auto file = fc.getResult();
                auto xml = juce::XmlDocument::parse (file);
                auto tree = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
                if (! tree.hasType ("STATE") || ! tree.getChildWithName ("CHAIN").isValid())
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                        "Preset not loaded", "Choose a valid Vocal Companion rack preset.");
                    return;
                }
                safe->proc.loadPresetTree (tree);
                safe->proc.currentPresetName = file.getFileNameWithoutExtension();
                if (safe->chainChanged) safe->chainChanged();
            });
    };
    rebuildPresetList();
}

void TopBar::rebuildPresetList()
{
    presetButton.setButtonText ("Browse: " + proc.currentPresetName);
    inGain.setValue (proc.masterInDb.load(), juce::dontSendNotification);
    outGain.setValue (proc.masterOutDb.load(), juce::dontSendNotification);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    presetButton.setBounds (r.removeFromLeft (280));
    r.removeFromLeft (6);
    saveBtn.setBounds (r.removeFromLeft (66));
    r.removeFromLeft (4);
    loadBtn.setBounds (r.removeFromLeft (56));
    r.removeFromLeft (12);
    inMeter.setBounds (r.removeFromLeft (10));
    r.removeFromLeft (4);
    inGain.setBounds (r.removeFromLeft (160));
    r.removeFromLeft (10);
    outGain.setBounds (r.removeFromLeft (160));
    r.removeFromLeft (4);
    outMeter.setBounds (r.removeFromLeft (10));
    r.removeFromLeft (12);
    compare->setBounds(r.removeFromLeft(200).withHeight(32));
    r.removeFromLeft(8);
    support.setBounds (r);
}

void TopBar::paint (juce::Graphics& g)
{
    const auto& s = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    g.fillAll (juce::Colour (s.chrome));
    g.setColour (juce::Colour (s.accent));
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (juce::Colour (s.accent).withAlpha (0.45f));
    g.drawLine (0, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f);
    g.setColour (juce::Colour (s.accent));
    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
    const int inX = 10 + 280 + 6 + 66 + 4 + 56 + 12;
    g.drawText ("IN", inX + 14, 4, 24, 10, juce::Justification::centredLeft);
    g.drawText ("OUT", inX + 184, 4, 28, 10, juce::Justification::centredLeft);
}
