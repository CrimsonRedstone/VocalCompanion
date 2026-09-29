#include "UI/PaletteDock.h"
#include "PluginProcessor.h"

namespace
{
const char* sources[] = { "All", "Factory", "Saved", "Favorites" };
const char* sounds[] = { "All sounds", "Clean", "Bright", "Ambient", "Cute", "Chaos", "Warm", "Lead", "Backing", "Rap", "Pitch", "Creative" };
}

EffectList::EffectList (VocalCompanionProcessor& p) : proc (p)
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &collapse, &tabFx, &tabPresets, &tabFun, &addEffect, &addVst,
             &addAeterna, &randomPreset, &randomizeAll, &search, &rows, &countLabel, &details, &skinHeading })
        addAndMakeVisible (component);

    collapse.setTooltip ("Collapse or expand the browser");
    collapse.onClick = [this] {
        collapsed = ! collapsed;
        collapse.setButtonText (collapsed ? ">" : "<");
        if (onCollapseChanged) onCollapseChanged();
        resized();
        repaint();
    };
    tabFx.onClick = [this] { setTab (Effects); };
    tabPresets.onClick = [this] { setTab (Presets); };
    tabFun.onClick = [this] { setTab (Fun); };
    search.setTextToShowWhenEmpty ("Search name, sound or effect...", juce::Colour (0xff8e99aa));
    search.setFont (juce::Font (juce::FontOptions (12.0f)));
    search.onTextChange = [this] { rebuildResults(); };
    search.onReturnKey = [this] { if (! visible.empty()) returnKeyPressed (0); };
    search.onEscapeKey = [this] { search.clear(); };

    for (int i = 0; i < 4; ++i)
    {
        auto* b = sourceButtons.add (new juce::TextButton (sources[i]));
        addAndMakeVisible (b);
        b->onClick = [this, i] { filter = (Filter) i; rebuildResults(); updateColours(); };
    }
    for (int i = 0; i < 12; ++i)
    {
        auto* b = tagButtons.add (new juce::TextButton (sounds[i]));
        addAndMakeVisible (b);
        b->onClick = [this, i] { soundFilter = i; rebuildResults(); updateColours(); };
    }
    rows.setRowSelectedOnMouseDown (false);
    rows.setModel (this);
    rows.setRowHeight (32);
    rows.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    rows.setOutlineThickness (0);
    rows.setMultipleSelectionEnabled (false);
    rows.setWantsKeyboardFocus (true);
    rows.setTooltip ("Click a preset to load. Arrow keys inspect; Enter loads. Star adds a favorite.");
    countLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    countLabel.setInterceptsMouseClicks (false, false);
    details.setFont (juce::Font (juce::FontOptions (12.0f)));
    details.setJustificationType (juce::Justification::topLeft);
    details.setInterceptsMouseClicks (false, false);
    skinHeading.setText ("APPEARANCE", juce::dontSendNotification);
    skinHeading.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));

    addEffect.onClick = [this] {
        auto pt = addEffect.getScreenBounds().getBottomLeft();
        if (onAddModule) onAddModule (pt.x, pt.y);
    };
    addVst.onClick = [this] { if (onAddVst) onAddVst(); };
    addAeterna.setTooltip ("Celestial choir, resonant glass and cathedral shimmer");
    addAeterna.onClick = [this] {
        {
            const juce::ScopedLock lock (proc.chainLock);
            proc.getChain().add (vc::ModuleType::Aeterna, 0);
            proc.rebindHostParams();
        }
        if (onChainChanged) onChainChanged();
        if (onRevealAdded) onRevealAdded();
        refresh();
    };
    randomPreset.onClick = [this] {
        std::vector<int> choices;
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].source == vc::RackPresetEntry::Factory
                || entries[(size_t) i].source == vc::RackPresetEntry::Style
                || entries[(size_t) i].source == vc::RackPresetEntry::Studio)
                choices.push_back (i);
        if (! choices.empty())
        {
            juce::Random rng;
            const auto entry = entries[(size_t) choices[(size_t) rng.nextInt ((int) choices.size())]];
            applyEntry (entry);
        }
    };
    randomizeAll.onClick = [this] {
        proc.randomizeEverything();
        if (onChainChanged) onChainChanged();
    };
    for (int i = 0; i < vc::kNumSkins; ++i)
    {
        auto* b = skinButtons.add (new juce::TextButton (vc::kSkins[i].name));
        addAndMakeVisible (b);
        b->onClick = [this, i] {
            proc.skinIndex = i;
            if (onSkinChanged) onSkinChanged();
            updateColours();
        };
    }
    refresh();
}

EffectList::~EffectList() { rows.setModel (nullptr); }

void EffectList::refresh()
{
    entries = vc::getRackPresetEntries();
    rebuildResults();
    updateColours();
    resized();
    repaint();
}

void EffectList::showPresets()
{
    if (collapsed)
    {
        collapsed = false;
        collapse.setButtonText ("<");
        if (onCollapseChanged) onCollapseChanged();
    }
    setTab (Presets);
    if (getPeer() != nullptr) search.grabKeyboardFocus();
}

void EffectList::setTab (Tab t)
{
    stopTimer(); dropIndex = -1;
    tab = t;
    rows.deselectAllRows();
    rows.setRowHeight (tab == Presets ? 48 : 32);
    rows.setRowSelectedOnMouseDown (tab != Effects);
    rows.setTooltip (tab == Effects ? "Drag the grip or effect name up/down to reorder. BYP bypasses; SOLO isolates." : "Click to load; arrows inspect; Enter loads; star adds a favorite.");
    rebuildResults();
    updateColours();
    resized();
    repaint();
}

bool EffectList::isFavorite (const vc::RackPresetEntry& entry) const
{
    return proc.isFavorite (entry.key) || proc.isFavorite (entry.legacyName)
           || proc.isFavorite (entry.name);
}

void EffectList::toggleFavorite (const vc::RackPresetEntry& entry)
{
    const bool was = isFavorite (entry);
    proc.favorites.removeString (entry.key);
    proc.favorites.removeString (entry.legacyName);
    proc.favorites.removeString (entry.name);
    if (! was) proc.favorites.add (entry.key);
    rebuildResults();
}

void EffectList::rebuildResults()
{
    visible.clear();
    auto words = juce::StringArray::fromTokens (search.getText().toLowerCase(), " ", "");
    words.removeEmptyStrings();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        const bool user = e.source == vc::RackPresetEntry::User;
        if (filter == Factory && user) continue;
        if (filter == Saved && ! user) continue;
        if (filter == Favorites && ! isFavorite (e)) continue;
        if (soundFilter > 0 && ! e.tags.containsIgnoreCase (sounds[soundFilter])) continue;
        const auto haystack = (e.name + " " + e.legacyName + " " + e.tags + " " + e.description).toLowerCase();
        bool match = true;
        for (const auto& word : words) if (! haystack.contains (word)) { match = false; break; }
        if (match) visible.push_back (i);
    }
    rows.updateContent();
    rows.repaint();
    countLabel.setText (juce::String ((int) visible.size()) + " presets  |  * favorite",
                        juce::dontSendNotification);
    int selected = -1;
    for (int i = 0; i < (int) visible.size(); ++i)
        if (entries[(size_t) visible[(size_t) i]].key == inspectedKey) selected = i;
    if (tab == Presets)
    {
        if (selected >= 0) rows.selectRow (selected, true, false);
        else rows.deselectAllRows();
        updateDetails (selected);
    }
}

void EffectList::applyEntry (const vc::RackPresetEntry& entry)
{
    const auto tree = vc::getRackPresetTree (entry);
    if (! tree.isValid())
    {
        details.setText ("This preset could not be read. Check the saved file.", juce::dontSendNotification);
        return;
    }
    inspectedKey = entry.key;
    proc.loadPresetTree (tree);
    if (onChainChanged) onChainChanged();
    repaint();
}

void EffectList::updateDetails (int row)
{
    if (tab != Presets) return;
    if (row < 0 || row >= (int) visible.size())
    {
        details.setText (visible.empty() ? "No matching presets. Change the filters or search."
                                         : "Click a preset to load it. Use the stars to keep favorites here.",
                         juce::dontSendNotification);
        return;
    }
    const auto& entry = entries[(size_t) visible[(size_t) row]];
    inspectedKey = entry.key;
    details.setText (entry.description, juce::dontSendNotification);
}

int EffectList::getNumRows()
{
    if (tab == Presets) return (int) visible.size();
    if (tab == Effects)
    {
        const juce::ScopedLock lock (proc.chainLock);
        return proc.getChain().size();
    }
    return 0;
}

void EffectList::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto ac = juce::Colour (vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)].accent);
    auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (0, 2);
    if (tab == Effects)
    {
        const juce::ScopedLock lock (proc.chainLock);
        auto* m = proc.getChain().get (row);
        if (m == nullptr) return;
        g.setColour (vc::typeColour (m->getType()).fill);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        g.setColour (m->isBypassed() ? juce::Colour (0xff8a95a5) : juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (11.5f).withStyle ("Bold")));
        g.drawText (m->getDisplayName(), bounds.reduced (8, 0).withTrimmedLeft (10).withTrimmedRight (78), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        for (int j = 0; j < 3; ++j) g.fillRect (7, height / 2 - 5 + j * 4, 6, 1);
        auto bypass = juce::Rectangle<int> (width - 78, 6, 34, height - 12);
        auto solo = juce::Rectangle<int> (width - 40, 6, 34, height - 12);
        g.setColour (m->isBypassed() ? ac.darker (0.4f) : juce::Colour (0x66000000));
        g.fillRoundedRectangle (bypass.toFloat(), 3.0f);
        g.setColour (m->isSoloed() ? juce::Colour (0xff977626) : juce::Colour (0x66000000));
        g.fillRoundedRectangle (solo.toFloat(), 3.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
        g.drawText ("BYP", bypass, juce::Justification::centred);
        g.drawText ("SOLO", solo, juce::Justification::centred);
        return;
    }
    if (tab != Presets || row < 0 || row >= (int) visible.size()) return;
    const auto& entry = entries[(size_t) visible[(size_t) row]];
    const bool active = proc.currentPresetName == entry.name;
    const auto entryColour = vc::presetColour(entry);
    g.setColour (juce::Colour(0xff11151c).interpolatedWith(entryColour,selected || active ? .32f : .13f));
    g.fillRoundedRectangle (bounds.toFloat(), 5.0f);
    g.setColour(entryColour.withAlpha(.8f));
    g.fillRoundedRectangle(3.f,8.f,3.f,(float)height-16.f,1.5f);
    const auto tags = juce::StringArray::fromTokens(entry.tags,"/","");
    for(int i=0;i<juce::jmin(3,tags.size());++i)
    {
        g.setColour(vc::presetCategoryColour(tags[i].trim()));
        g.fillRoundedRectangle((float)width-38-i*9, (float)height-7,6.f,2.f,1.f);
    }
    if (active)
    {
        g.setColour (entryColour.brighter(.4f));
        g.fillRoundedRectangle (3.0f, 9.0f, 3.0f, (float) height - 18.0f, 1.5f);
    }
    g.setColour (juce::Colour (0xffedf2f8));
    g.setFont (juce::Font (juce::FontOptions (12.5f).withStyle ("Bold")));
    g.drawText (entry.name, 12, 6, width - 46, 18, juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xff9ba8b9));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText (entry.tags, 12, 25, width - 46, 15, juce::Justification::centredLeft, true);
    juce::Path star;
    star.addStar ({ (float) width - 18.0f, (float) height * 0.5f }, 5, 3.8f, 8.0f);
    g.setColour (isFavorite (entry) ? entryColour.brighter(.3f) : entryColour.withAlpha(.65f));
    if (isFavorite (entry)) g.fillPath (star);
    else g.strokePath (star, juce::PathStrokeType (1.2f));
}

void EffectList::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (tab == Presets && row >= 0 && row < (int) visible.size())
    {
        const auto entry = entries[(size_t) visible[(size_t) row]];
        if (e.x >= rows.getVisibleContentWidth() - 34) toggleFavorite (entry);
        else applyEntry (entry);
    }
    else if (tab == Effects)
    {
        bool changed = false;
        {
            const juce::ScopedLock lock (proc.chainLock);
            if (auto* m = proc.getChain().get (row))
            {
                const int width = rows.getVisibleContentWidth();
                if (e.x >= width - 40) { m->setSoloed (! m->isSoloed()); changed = true; }
                else if (e.x >= width - 78) { m->setBypassed (! m->isBypassed()); changed = true; }
            }
        }
        if (changed && onChainChanged) onChainChanged();
    }
}

void EffectList::selectedRowsChanged (int row) { updateDetails (row); }
void EffectList::returnKeyPressed (int row)
{
    if (tab == Presets && row >= 0 && row < (int) visible.size())
    {
        const auto entry = entries[(size_t) visible[(size_t) row]];
        applyEntry (entry);
    }
}

void EffectList::updateColours()
{
    const auto& skin = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    const auto ac = juce::Colour (skin.accent);
    const auto chrome = juce::Colour (skin.chrome);
    auto style = [&] (juce::TextButton& b, bool active) {
        b.setColour (juce::TextButton::buttonColourId, active ? ac.darker (0.48f) : chrome.brighter (0.08f));
        b.setColour (juce::TextButton::textColourOffId, active ? juce::Colours::white : juce::Colour (0xffc8d0de));
    };
    style (tabFx, tab == Effects); style (tabPresets, tab == Presets); style (tabFun, tab == Fun);
    const juce::uint32 sourceColours[] = {0xff59b6c7,0xff839cf0,0xff6bc9a3,0xffedc369};
    auto coloured = [] (juce::TextButton& b, juce::Colour c, bool selected) {
        b.setColour (juce::TextButton::buttonColourId, c.darker(selected ? .45f : 1.4f));
        b.setColour (juce::TextButton::textColourOffId, selected ? juce::Colours::white : c.brighter(.35f));
    };
    for (int i = 0; i < sourceButtons.size(); ++i) coloured (*sourceButtons[i],juce::Colour(sourceColours[i]),(int)filter==i);
    for (int i = 0; i < tagButtons.size(); ++i) coloured (*tagButtons[i],vc::presetCategoryColour(sounds[i]),soundFilter==i);
    for (auto* b : { &collapse, &addEffect, &addVst, &addAeterna, &randomPreset, &randomizeAll }) style (*b, false);
    for (int i = 0; i < skinButtons.size(); ++i)
    {
        auto colour = juce::Colour (vc::kSkins[i].accent);
        skinButtons[i]->setButtonText ((proc.skinIndex == i ? "* " : "") + juce::String (vc::kSkins[i].name));
        skinButtons[i]->setColour (juce::TextButton::buttonColourId, colour);
        skinButtons[i]->setColour (juce::TextButton::textColourOffId,
            colour.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff10141b) : juce::Colours::white);
    }
    search.setColour (juce::TextEditor::backgroundColourId, chrome.darker (0.3f));
    search.setColour (juce::TextEditor::outlineColourId, ac.withAlpha (0.3f));
    search.setColour (juce::TextEditor::focusedOutlineColourId, ac);
    search.setColour (juce::TextEditor::textColourId, juce::Colour (0xffedf2f8));
    countLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9ba8b9));
    details.setColour (juce::Label::textColourId, juce::Colour (0xffc8d0de));
    skinHeading.setColour (juce::Label::textColourId, ac);
    rows.repaint();
}

void EffectList::resized()
{
    auto r = getLocalBounds().reduced (8);
    collapse.setBounds (getWidth() - 22, 2, 20, 18);
    const bool open = ! collapsed;
    for (auto* b : { &tabFx, &tabPresets, &tabFun }) b->setVisible (open);
    const bool preset = open && tab == Presets;
    search.setVisible (preset); countLabel.setVisible (preset); details.setVisible (preset);
    for (auto* b : sourceButtons) b->setVisible (preset);
    for (auto* b : tagButtons) b->setVisible (preset);
    rows.setVisible (open && tab != Fun);
    addEffect.setVisible (open && tab == Effects); addVst.setVisible (open && tab == Effects);
    addAeterna.setVisible (open && tab == Fun);
    randomPreset.setVisible (open && tab == Fun); randomizeAll.setVisible (open && tab == Fun);
    skinHeading.setVisible (open && tab == Fun);
    for (auto* b : skinButtons) b->setVisible (open && tab == Fun);
    detailsBounds = {};
    if (! open) return;
    r.removeFromTop (16);
    auto tabs = r.removeFromTop (28);
    const int third = tabs.getWidth() / 3;
    tabFx.setBounds (tabs.removeFromLeft (third).reduced (1, 0));
    tabPresets.setBounds (tabs.removeFromLeft (third).reduced (1, 0));
    tabFun.setBounds (tabs.reduced (1, 0));
    r.removeFromTop (10);
    if (tab == Effects)
    {
        auto bottom = r.removeFromBottom (28);
        addEffect.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0));
        addVst.setBounds (bottom.reduced (2, 0));
        r.removeFromBottom (8);
        rows.setBounds (r);
    }
    else if (tab == Presets)
    {
        search.setBounds (r.removeFromTop (28)); r.removeFromTop (6);
        auto sourceRow = r.removeFromTop (24);
        const int w = sourceRow.getWidth() / 4;
        for (auto* b : sourceButtons) b->setBounds (sourceRow.removeFromLeft (w).reduced (1, 0));
        r.removeFromTop (6);
        for (int row = 0; row < 3; ++row)
        {
            auto tagRow = r.removeFromTop (22);
            const int tw = tagRow.getWidth() / 4;
            for (int col = 0; col < 4; ++col)
                tagButtons[row * 4 + col]->setBounds (tagRow.removeFromLeft (tw).reduced (1, 1));
        }
        countLabel.setBounds (r.removeFromTop (22));
        detailsBounds = r.removeFromBottom (62);
        details.setBounds (detailsBounds.reduced (6));
        r.removeFromBottom (6);
        rows.setBounds (r);
    }
    else
    {
        addAeterna.setBounds (r.removeFromTop (32)); r.removeFromTop (8);
        randomPreset.setBounds (r.removeFromTop (30)); r.removeFromTop (6);
        randomizeAll.setBounds (r.removeFromTop (30)); r.removeFromTop (14);
        skinHeading.setBounds (r.removeFromTop (20)); r.removeFromTop (4);
        const int w = r.getWidth() / 3;
        for (int i = 0; i < skinButtons.size(); ++i)
            skinButtons[i]->setBounds (r.getX() + i % 3 * w, r.getY() + i / 3 * 32, w - 4, 28);
    }
}

void EffectList::paint (juce::Graphics& g)
{
    const auto& skin = vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)];
    g.fillAll (juce::Colour (skin.chrome));
    g.setColour (juce::Colour (skin.accent).withAlpha (0.18f));
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
    if (! detailsBounds.isEmpty())
    {
        g.setColour (juce::Colour (0xff10141b));
        g.fillRoundedRectangle (detailsBounds.toFloat(), 5.0f);
    }
    // All tab content is laid out below the tabs; no painted heading underneath them.
}

// UUID descriptions also allow dragging between cards and the effect list.
juce::var EffectList::getDragSourceDescription (const juce::SparseSet<int>& selected)
{
    if (tab != Effects || selected.size() != 1) return {};
    const juce::ScopedLock lock (proc.chainLock);
    if (auto* m = proc.getChain().get (selected[0])) return "module:" + m->instanceId.toString();
    return {};
}
bool EffectList::isInterestedInDragSource (const SourceDetails& d)
{
    return tab == Effects && !collapsed && d.description.toString().startsWith ("module:");
}
int EffectList::insertionAt (int y) const
{
    const juce::ScopedLock lock (proc.chainLock);
    const int offset = rows.getViewport()->getViewPositionY();
    return juce::jlimit (0, proc.getChain().size(), (y - rows.getY() + offset + 16) / 32);
}
void EffectList::itemDragMove (const SourceDetails& d)
{
    if (!isInterestedInDragSource (d)) return;
    dragY = d.localPosition.y;
    dropIndex = insertionAt (dragY);
    startTimerHz (30);
    repaint();
}
void EffectList::timerCallback()
{
    if (dropIndex < 0) { stopTimer(); return; }
    auto* view = rows.getViewport();
    const int delta = dragY < rows.getY() + 22 ? -12 : dragY > rows.getBottom() - 22 ? 12 : 0;
    if (delta != 0)
    {
        view->setViewPosition (0, juce::jmax (0, view->getViewPositionY() + delta));
        dropIndex = insertionAt (dragY);
        repaint();
    }
}

void EffectList::itemDropped (const SourceDetails& d)
{
    stopTimer();
    if (!isInterestedInDragSource (d)) return;
    {
        const juce::ScopedLock lock (proc.chainLock);
        const auto id = d.description.toString().substring (7);
        for (int i = 0; i < proc.getChain().size(); ++i)
            if (proc.getChain().get (i)->instanceId.toString() == id)
            {
                proc.getChain().move (i, insertionAt (d.localPosition.y));
                proc.rebindHostParams();
                break;
            }
    }
    dropIndex = -1;
    if (onChainChanged) onChainChanged();
    refresh();
}
void EffectList::paintOverChildren (juce::Graphics& g)
{
    if (dropIndex < 0 || tab != Effects) return;
    g.reduceClipRegion (rows.getBounds());
    const int y = rows.getY() + dropIndex * 32 - rows.getViewport()->getViewPositionY();
    g.setColour (juce::Colour (vc::kSkins[juce::jlimit (0, vc::kNumSkins - 1, proc.skinIndex)].accent));
    g.fillRoundedRectangle ((float) rows.getX(), (float) y - 2, (float) rows.getWidth(), 4.0f, 2.0f);
}
