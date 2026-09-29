#include "PluginEditor.h"
#include "BrandingAssets.h"
#include "Host/ExternalHost.h"

VocalCompanionEditor::VocalCompanionEditor (VocalCompanionProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      top (p, [this] { proc.rebindHostParams(); rack.rebuild(); list.refresh(); top.rebuildPresetList(); }),
      rack (p),
      list (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    root.addAndMakeVisible (top);
    root.addAndMakeVisible (rack);
    root.addAndMakeVisible (list);

    rack.onChanged = [this] { list.refresh(); };
    top.onBrowsePresets = [this] { list.showPresets(); };
    list.onCollapseChanged = [this] { resized(); };
    list.onRevealAdded = [this] { rack.revealFirst(); };
    list.onChainChanged = [this] { rack.rebuild(); list.refresh(); top.rebuildPresetList(); };
    list.onSkinChanged = [this] {
        applySkin();
    };
    list.onAddModule = [this] (int sx, int sy) {
        rack.showAdd (proc.getChain().size(), sx, sy);
    };
    list.onAddVst = [this] {
        vc::showPluginScannerDialog ([this] (juce::PluginDescription d) {
            const juce::ScopedLock sl (proc.chainLock);
            const int idx = proc.getChain().add (vc::ModuleType::External);
            if (auto* ext = dynamic_cast<vc::ExternalModule*> (proc.getChain().get (idx)))
            { ext->loadFromDescription (d); ext->initialiseSnapshots(); }
            juce::MessageManager::callAsync ([this] {
                proc.rebindHostParams();
                rack.rebuild();
                list.refresh();
            });
        });
    };

    applySkin();
    root.setName ("vc-root");
    setResizable (true, true);
    setResizeLimits (1280, 520, 3840, 2160);
    setSize (1280, 520);
}

VocalCompanionEditor::~VocalCompanionEditor()
{
    setLookAndFeel (nullptr);
}

void VocalCompanionEditor::applySkin()
{
    const auto& s = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    auto ac = juce::Colour (s.accent);
    auto bg = juce::Colour (s.bg);
    auto ch = juce::Colour (s.chrome);
    lnf.accent = ac;
    lnf.panel  = bg;
    lnf.chrome = ch;
    lnf.setColour (juce::ResizableWindow::backgroundColourId, bg);
    lnf.setColour (juce::ComboBox::backgroundColourId, ch.brighter (0.06f));
    lnf.setColour (juce::ComboBox::outlineColourId, ac.withAlpha (0.45f));
    lnf.setColour (juce::TextButton::buttonColourId, ch.brighter (0.08f));
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, ac.withAlpha (0.22f));
    lnf.setColour (juce::HyperlinkButton::textColourId, ac);
    sendLookAndFeelChange();
    top.repaint();
    list.refresh();
    rack.rebuild();
    rack.repaint();
    repaint();
}

void VocalCompanionEditor::paint (juce::Graphics& g)
{
    const auto& s = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    g.fillAll (juce::Colour (s.bg));
}

void VocalCompanionEditor::resized()
{
    const float dw = 1280.0f, dh = 520.0f;
    const float s = juce::jmin ((float) getWidth() / dw, (float) getHeight() / dh);
    // Uniform control scale, responsive canvas: fill arbitrary host aspect ratios.
    root.setTransform (juce::AffineTransform::scale (s));
    root.setBounds (0, 0, (int) std::ceil (getWidth() / s), (int) std::ceil (getHeight() / s));
    auto r = root.getLocalBounds();
    top.setBounds (r.removeFromTop (48));
    list.setBounds (r.removeFromLeft (list.preferredWidth()));
    rack.setBounds (r);
}

void VocalCompanionEditor::parentHierarchyChanged() { updateWindowIcon(); }
void VocalCompanionEditor::visibilityChanged() { updateWindowIcon(); }
void VocalCompanionEditor::updateWindowIcon()
{
    // Only brand our standalone window, never the DAW's owning window.
    if (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* peer = getPeer())
            peer->setIcon (juce::ImageCache::getFromMemory (
                BrandingAssets::VocalCompanion256_png, BrandingAssets::VocalCompanion256_pngSize));
}
