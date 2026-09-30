#include "UI/RackComponent.h"
#include "UI/AddMenu.h"
#include "DSP/Common.h"

namespace
{
    constexpr int kPlus = 12;
    constexpr int kGap  = 1;
}

RackComponent::RackComponent (VocalCompanionProcessor& p) : proc (p)
{
    viewport.setViewedComponent (&strip, false);
    viewport.setScrollBarsShown (true, true);
    addAndMakeVisible (viewport);
    strip.addMouseListener (this, false);
    rebuild();
}

RackComponent::~RackComponent()
{
    strip.removeMouseListener (this);
}

void RackComponent::insertType (vc::ModuleType type, int at)
{
    {
        const juce::ScopedLock sl (proc.chainLock);
        proc.checkpoint();
        proc.getChain().add (type, at);
    }
    proc.rebindHostParams();
    rebuild();
    if (onChanged) onChanged();
}

void RackComponent::replaceType (int at, vc::ModuleType type)
{
    {
        const juce::ScopedLock sl (proc.chainLock);
        proc.checkpoint();
        proc.getChain().replace (at, type);
    }
    proc.rebindHostParams();
    rebuild();
    if (onChanged) onChanged();
}

void RackComponent::showAdd (int at, int sx, int sy)
{
    showAddModuleMenu (sx, sy, [this, at] (vc::ModuleType t) { insertType (t, at); });
}

void RackComponent::showReplace (int at, int sx, int sy)
{
    showAddModuleMenu (sx, sy, [this, at] (vc::ModuleType t) { replaceType (at, t); });
}

void RackComponent::rebuild()
{
    cards.clear();
    pluses.clear();
    const juce::ScopedLock sl (proc.chainLock);
    const int n = proc.getChain().size();
    for (int i = 0; i <= n; ++i)
    {
        auto* plus = pluses.add (new IconButton (IconButton::Plus,
            juce::Colour (vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)].accent)));
        plus->onClick = [this, i] {
            auto p = pluses[i] != nullptr ? pluses[i]->getScreenBounds().getCentre()
                                          : juce::Point<int> (0, 0);
            showAdd (i, p.x, p.y);
        };
        strip.addAndMakeVisible (plus);
        if (i == n) break;
        auto* m = proc.getChain().get (i);
        auto* card = cards.add (new ModuleCard (*m, proc,
            [this, i] {
                {
                    const juce::ScopedLock lock (proc.chainLock);
                    proc.checkpoint();
                    proc.getChain().remove (i);
                }
                proc.rebindHostParams();
                rebuild();
                if (onChanged) onChanged();
            },
            [this] {
                if (onChanged) onChanged();
                repaint();
            }));
        card->onReplace = [this, i] (int sx, int sy) { showReplace (i, sx, sy); };
        card->onDuplicate=[this,i]{
            {const juce::ScopedLock lock(proc.chainLock);proc.checkpoint();proc.getChain().duplicate(i);}
            proc.rebindHostParams();rebuild();if(onChanged)onChanged();
        };
        strip.addAndMakeVisible (card);
    }
    resized();
    repaint();
}

void RackComponent::resized()
{
    viewport.setBounds (getLocalBounds());
    int cardH = 0;
    for (auto* card : cards)
        cardH = juce::jmax (cardH, card->preferredHeight());
    const int h = juce::jmax (cardH + 16, viewport.getHeight() - 14);
    const int cardW = ModuleCard::preferredWidth;
    const int n = cards.size();
    const int need = 16 + n * (cardW + kPlus + kGap * 2) + kPlus;
    strip.setSize (juce::jmax (getWidth(), need), h);
    int x = 8;
    const int plusY = 8 + 44;
    for (int i = 0; i < n; ++i)
    {
        pluses[i]->setBounds (x, plusY, kPlus, kPlus);
        x += kPlus + kGap;
        cards[i]->setBounds (x, 8, cardW, cards[i]->preferredHeight());
        x += cardW + kGap;
    }
    if (pluses.size() > n)
        pluses[n]->setBounds (juce::jmax (x, 8), plusY, kPlus, kPlus);
}

void RackComponent::paint (juce::Graphics& g)
{
    const auto& s = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    g.fillAll (juce::Colour (s.bg));
    if (dropIndex >= 0)
    {
        g.setColour (juce::Colour (0xff3ec8e0));
        const int cardW = cards.isEmpty() ? 160 : cards[0]->getWidth();
        const int x = 8 + dropIndex * (cardW + kPlus + kGap * 2) + kPlus / 2
                      - viewport.getViewPositionX();
        g.fillRect (x - 1, 8, 2, getHeight() - 16);
    }
}

void RackComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;
    // empty strip only — cards and knobs consume their own clicks
    if (e.eventComponent == &strip || e.eventComponent == this)
        showAdd (proc.getChain().size(), e.getScreenX(), e.getScreenY());
}

bool RackComponent::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("module:");
}

int RackComponent::indexForX (int x) const
{
    const int local = x + viewport.getViewPositionX();
    if (cards.isEmpty()) return 0;
    for (int i = 0; i < cards.size(); ++i)
        if (local < cards[i]->getBounds().getCentreX()) return i;
    return cards.size();
}

void RackComponent::itemDragMove (const SourceDetails& d)
{
    dropIndex = indexForX (d.localPosition.x);
    repaint();
}

void RackComponent::itemDropped (const SourceDetails& d)
{
    const int at = indexForX (d.localPosition.x);
    const auto desc = d.description.toString();
    {
        const juce::ScopedLock sl (proc.chainLock);
        if (desc.startsWith ("module:"))
        {
            const auto id = desc.fromFirstOccurrenceOf (":", false, false);
            int from = -1;
            for (int i = 0; i < proc.getChain().size(); ++i)
                if (proc.getChain().get (i)->instanceId.toString() == id)
                    from = i;
            if (from >= 0){proc.checkpoint();proc.getChain().move (from, at);}
        }
    }
    dropIndex = -1;
    proc.rebindHostParams();
    rebuild();
    if (onChanged) onChanged();
}

void RackComponent::revealLast()
{
    if (! cards.isEmpty())
        viewport.setViewPosition (juce::jmax (0, cards.getLast()->getRight() + 16 - viewport.getViewWidth()), 0);
}
