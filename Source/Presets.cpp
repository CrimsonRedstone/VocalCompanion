#include "Presets.h"
#include <iterator>

namespace vc
{

static const FactoryPreset kFactory[] = {
    { "Vocal Lead Polish",
      R"(<STATE in='0' out='0' lim='1' name='Vocal Lead Polish'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='3' mode='1' drive='0.25'/>
  <MODULE type='deesser' bypass='0' harsh='-20' sib='-16' range='12' sense='0.8'/>
  <MODULE type='fetcomp' bypass='0' input='4' output='0' thresh='-16' ratio='8' attack='0.3' release='180' mix='1'/>
  <MODULE type='dyneq' bypass='0' g3='-1.5' g4='-2' air='3.5' dyn='0.55'/>
  <MODULE type='chorus' bypass='0' rate='0.6' depth='1.8' mix='0.22'/>
  <MODULE type='delay' bypass='0' sync='1' div='0' mix='0.14' fb='0.25'/>
</CHAIN></STATE>)" },
    { "Natural Voice",
      R"(<STATE in='0' out='0' lim='1' name='Natural Voice'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='0' mode='1' drive='0.15'/>
  <MODULE type='deesser' bypass='0' harsh='-24' sib='-20' range='8' sense='0.55'/>
  <MODULE type='optocomp' bypass='0' thresh='-18' ratio='3' attack='18' release='320' makeup='3'/>
  <MODULE type='dyneq' bypass='0' air='2' dyn='0.3'/>
  <MODULE type='imager' bypass='0' lo='0' hi='45' xover='250' trans='80'/>
  <MODULE type='reverb' bypass='0' size='0.32' mix='0.12' duck='0.8'/>
</CHAIN></STATE>)" },
    { "Intimate Acoustic",
      R"(<STATE in='0' out='0' lim='1' name='Intimate Acoustic'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='2' mode='0' drive='0'/>
  <MODULE type='optocomp' bypass='0' thresh='-22' ratio='2.5' attack='25' release='400' makeup='2'/>
  <MODULE type='dyneq' bypass='0' g1='-2' g2='1' air='1.5'/>
  <MODULE type='reverb' bypass='0' size='0.22' mix='0.1' duck='0.65'/>
</CHAIN></STATE>)" },
    { "Radio Vocal",
      R"(<STATE in='0' out='0' lim='1' name='Radio Vocal'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='4' mode='2' drive='0.45'/>
  <MODULE type='deesser' bypass='0' harsh='-18' sib='-14' range='14' sense='0.85'/>
  <MODULE type='fetcomp' bypass='0' input='8' thresh='-12' ratio='12' attack='0.08' release='90' mix='1'/>
  <MODULE type='dyneq' bypass='0' g2='-3' g3='4' g4='-1' air='4' dyn='0.4'/>
</CHAIN></STATE>)" },
    { "Wide Chorus Lead",
      R"(<STATE in='0' out='0' lim='1' name='Wide Chorus Lead'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='1' mode='1' drive='0.2'/>
  <MODULE type='fetcomp' bypass='0' thresh='-18' ratio='8' mix='0.85'/>
  <MODULE type='chorus' bypass='0' rate='0.7' depth='3' voices='4' mix='0.45'/>
  <MODULE type='imager' bypass='0' lo='0' hi='65' xover='250' trans='70'/>
  <MODULE type='delay' bypass='0' sync='1' ping='1' mix='0.22'/>
</CHAIN></STATE>)" },
    { "De-essed Harmony",
      R"(<STATE in='0' out='0' lim='1' name='De-essed Harmony'>
<CHAIN>
  <MODULE type='deesser' bypass='0' harsh='-16' sib='-12' range='16' sense='1'/>
  <MODULE type='optocomp' bypass='0' thresh='-16' ratio='4' makeup='4'/>
  <MODULE type='chorus' bypass='0' mix='0.18'/>
  <MODULE type='reverb' bypass='0' size='0.5' mix='0.2' duck='0.75'/>
</CHAIN></STATE>)" },
    { "Dirty Tube Verse",
      R"(<STATE in='0' out='0' lim='1' name='Dirty Tube Verse'>
<CHAIN>
  <MODULE type='gain' bypass='0' gain='6' mode='2' drive='0.7'/>
  <MODULE type='fetcomp' bypass='0' input='6' thresh='-14' ratio='20' attack='0.02' release='120'/>
  <MODULE type='deesser' bypass='0' range='8'/>
  <MODULE type='autopan' bypass='0' rate='0.25' width='0.35'/>
</CHAIN></STATE>)" },
    { "Airy Chorus Hook",
      R"(<STATE in='0' out='0' lim='1' name='Airy Chorus Hook'>
<CHAIN>
  <MODULE type='gain' bypass='0' mode='1' drive='0.2'/>
  <MODULE type='optocomp' bypass='0' thresh='-20' ratio='3'/>
  <MODULE type='dyneq' bypass='0' air='5' airhz='13000' dyn='0.25'/>
  <MODULE type='chorus' bypass='0' mix='0.3' depth='2.2'/>
  <MODULE type='phaser' bypass='0' mix='0.18' rate='0.2'/>
  <MODULE type='delay' bypass='0' sync='1' div='1' mix='0.16'/>
  <MODULE type='reverb' bypass='0' size='0.55' mix='0.22' duck='0.85'/>
</CHAIN></STATE>)" }
};

juce::StringArray getFactoryPresetNames()
{
    juce::StringArray n;
    for (auto& p : kFactory)
        n.add (p.name);
    return n;
}

juce::ValueTree getFactoryPreset (int index)
{
    if (index < 0 || index >= (int) (sizeof (kFactory) / sizeof (kFactory[0])))
        return {};
    if (auto xml = juce::XmlDocument::parse (kFactory[index].xml))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

juce::ValueTree getFactoryPreset (const juce::String& name)
{
    // Legacy aliases remain loadable; they are no longer displayed in the catalog.
    if (name == "Vocaloid Lead Polish") return getFactoryPreset(0);
    if (name == "Synth V Natural") return getFactoryPreset(1);
    for (int i = 0; i < (int) (sizeof (kFactory) / sizeof (kFactory[0])); ++i)
        if (name == kFactory[i].name)
            return getFactoryPreset (i);
    return {};
}

juce::File getUserPresetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Crimson Redstone")
                   .getChildFile ("Vocal Companion")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray getUserPresetNames()
{
    juce::StringArray n;
    for (auto& f : getUserPresetDir().findChildFiles (juce::File::findFiles, false, "*.vcpreset"))
        n.add (f.getFileNameWithoutExtension());
    n.sortNatural();
    return n;
}

bool saveUserPreset (const juce::String& name, const juce::ValueTree& state)
{
    auto file = getUserPresetDir().getChildFile (name + ".vcpreset");
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

juce::ValueTree loadUserPreset (const juce::String& name)
{
    auto file = getUserPresetDir().getChildFile (name + ".vcpreset");
    if (auto xml = juce::XmlDocument::parse (file))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

} // namespace vc

#include "StyleCatalog.h"
#include "StudioPresets.h"

namespace vc
{
juce::Colour presetCategoryColour (const juce::String& name)
{
    const char* names[] = {"Clean","Bright","Ambient","Cute","Chaos","Warm","Lead","Backing","Rap","Pitch","Creative"};
    const juce::uint32 colours[] = {0xff67d3b4,0xffebce70,0xff9b9af1,0xffef9bc7,0xffec7f8c,0xffdfab7b,0xff78bbee,0xff83b59e,0xffd59359,0xffb99bea,0xff79cbd7};
    for(int i=0;i<11;++i) if(name.containsIgnoreCase(names[i])) return juce::Colour(colours[i]);
    return juce::Colour(0xff91a8ba);
}
juce::Colour presetColour (const RackPresetEntry& entry)
{
    auto tags=juce::StringArray::fromTokens(entry.tags,"/","");
    auto c=presetCategoryColour(tags.isEmpty()?"":tags[0].trim());
    if(tags.size()>1)c=c.interpolatedWith(presetCategoryColour(tags[1].trim()),.45f);
    // A small stable variation distinguishes nearby presets without losing their family hue.
    const auto hash=(unsigned)entry.key.hashCode();
    return c.withRotatedHue(((int)(hash%17)-8)*.002f).withMultipliedBrightness(.92f+(hash%9)*.02f);
}

std::vector<RackPresetEntry> getRackPresetEntries()
{
    std::vector<RackPresetEntry> entries;
    entries.push_back ({ RackPresetEntry::Init, -1, "init:empty", "Empty Rack", "Empty",
                        "Utility", "Start with an empty effect chain." });
    const char* factoryInfo[] = {
        "De-essing, FET compression, bright EQ, chorus and delay.",
        "Gentle leveling, airy EQ, stereo width and a small room.",
        "Clean gain, slow compression, warm EQ and subtle reverb.",
        "Driven gain, firm compression and forward vocal EQ.",
        "Four-voice chorus, stereo width and a synced delay.",
        "Strong de-essing with leveling, light chorus and reverb.",
        "Tube drive, aggressive FET compression and slow auto pan.",
        "Bright EQ, chorus, phaser, delay and ducked reverb."
    };
    const char* factoryTags[] = {
        "Lead Vocal / Bright", "Lead Vocal / Clean", "Lead Vocal / Clean",
        "Lead Vocal / Chaos", "Lead Vocal / Bright", "Backing Vocal / Clean",
        "Lead Vocal / Chaos", "Lead Vocal / Bright"
    };
    auto names = getFactoryPresetNames();
    for (int i = 0; i < names.size(); ++i)
        entries.push_back ({ RackPresetEntry::Factory, i, "factory:" + juce::String (i),
                            i == 0 ? juce::String ("Vocal Lead Polish") : names[i],
                            i == 0 ? juce::String("Vocaloid Lead Polish") : i == 1 ? juce::String("Synth V Natural") : names[i],
                            factoryTags[i], factoryInfo[i] });

    // Old XML and names remain readable; these are browser display names only.
    const char* styleNames[] = {
        "Bright Character Lead", "Airy Stereo Shimmer", "Soft Ambient Backing",
        "Driven Pop Punch", "Heavy Vocal Grit", "Warm Soft Backing",
        "Hard Snap Phaser", "Distorted Midrange", "Tight Presence Lead",
        "Saturated Short Delay", "Warm Acoustic Room", "Dense Chorus Lead",
        "Chopped Phaser Echo", "Dark Ambient Wash", "Clear Wide Lead",
        "Forward Vowel Colour", "Bright Vowel Colour", "Air and Breath",
        "Metallic Ring Voice", "Crushed Ring Echo", "Square-Wave Chops",
        "Distant Dark Reverb", "Wide Harmony Stack", "Bright Chorus Stack",
        "Wide Club Drive", "High Vowel Colour", "Low Vowel Grit", "Chopped Vowel Echo",
        "Hard Scale Correction", "Natural Pitch Polish"
    };
    static_assert (std::size (styleNames) == kNumStyles);
    const char* styleInfo[] = {
        "Vowel colour, bright EQ, de-essing and FET compression.",
        "Airy EQ, light chorus, width and ducked reverb.",
        "Gentle input, optical compression, airy EQ and a long reverb.",
        "Driven gain, fast compression, vocal EQ and limiting.",
        "Strong drive, aggressive compression, midrange EQ and low vowel colour.",
        "Clean gain, slow compression, dark air EQ and a small reverb.",
        "Hard pitch correction, focused EQ, slow phaser and compression.",
        "Driven gain, midrange EQ, heavy compression and de-essing.",
        "Mild drive, de-essing, tight FET compression and presence EQ.",
        "Driven gain, pitch correction, fast compression and synced eighth-note delay.",
        "Clean gain, optical compression, body EQ and a small room.",
        "Drive, pitch correction, compression, bright EQ and four-voice chorus.",
        "Driven gain, narrow vocal EQ, fast phaser, tremolo and delay.",
        "Low vowel colour, dark EQ, delay and a large reverb.",
        "De-essing, optical compression, bright EQ, stereo width and a small room.",
        "Raised vowel colour, midrange EQ and FET compression.",
        "Raised vowel colour, air, de-essing and optical compression.",
        "Breath layer, de-essing, optical compression and an air boost.",
        "Pitch correction, ring modulation, band-limiting EQ and compression.",
        "Drive, bit reduction, ring modulation, delay and limiting.",
        "Bit reduction, square tremolo, ping-pong delay and high-pass EQ.",
        "Dark EQ, slow chorus, delay and a large reverb.",
        "Optical compression, four-voice chorus, width and light reverb.",
        "Raised vowel colour, air, de-essing, compression, chorus and width.",
        "Driven gain, compression, bright EQ, width, side EQ and limiting.",
        "High vowel colour, fast pitch correction, air and a short delay.",
        "Low vowel colour, warm excitation, dark EQ and compression.",
        "Raised vowel colour, fast correction, square tremolo and delay.",
        "Fast scale correction with de-essing, compression and focused EQ.",
        "Slower pitch correction with flex and humanize, compression and air EQ.",
    };
    static_assert (std::size (styleInfo) == kNumStyles);

    for (int i = 0; i < kNumStyles; ++i)
    {
        const auto& s = kStyles[i];
        entries.push_back ({ RackPresetEntry::Style, i, "style:" + juce::String (i),
                            styleNames[i], s.name, juce::String (s.type) + " / " + s.color,
                            styleInfo[i] });
    }
    for (int i = 0; i < (int) std::size (kStudio); ++i)
    {
        const auto& s = kStudio[i];
        entries.push_back ({ RackPresetEntry::Studio, i, "studio:" + juce::String (s.key),
                            s.name, s.name, s.tags, s.description, s.audience });
    }
    // All legacy racks are usable on either source; this acoustic rack targets raw recordings.
    entries[3].audience = 1;
    for (auto& e : entries)
    {
        if (e.name.containsIgnoreCase ("Warm")) e.tags += " / Warm";
        if (e.name.containsIgnoreCase ("Pitch") || e.name.containsIgnoreCase ("Correction")) e.tags += " / Pitch";
        if (e.tags.containsIgnoreCase ("Chaos") || e.tags.containsIgnoreCase ("Robotic")
            || e.tags.containsIgnoreCase ("Chopped")) e.tags += " / Creative";
    }
    for (const auto& name : getUserPresetNames())
        entries.push_back ({ RackPresetEntry::User, -1, "user:" + name, name, name,
                            "Saved rack", "Your saved effect chain." });
    return entries;
}

juce::ValueTree getRackPresetTree (const RackPresetEntry& entry)
{
    juce::ValueTree tree;
    switch (entry.source)
    {
        case RackPresetEntry::Init:
            tree = juce::ValueTree ("STATE");
            tree.appendChild (juce::ValueTree ("CHAIN"), nullptr);
            break;
        case RackPresetEntry::Factory: tree = getFactoryPreset (entry.index); break;
        case RackPresetEntry::Style:   tree = getStyleTree (entry.index); break;
        case RackPresetEntry::Studio:
            if (entry.index >= 0 && entry.index < (int) std::size (kStudio))
                if (auto xml = juce::XmlDocument::parse (kStudio[entry.index].xml))
                    tree = juce::ValueTree::fromXml (*xml);
            break;
        case RackPresetEntry::User:    tree = loadUserPreset (entry.legacyName); break;
    }
    if (tree.isValid()) tree.setProperty ("name", entry.name, nullptr);
    return tree;
}
}
