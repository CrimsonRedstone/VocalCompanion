#pragma once
#include <JuceHeader.h>
#include <vector>

namespace vc
{

struct FactoryPreset
{
    const char* name;
    const char* xml;
};

juce::StringArray getFactoryPresetNames();
juce::ValueTree   getFactoryPreset (int index);
juce::ValueTree   getFactoryPreset (const juce::String& name);

juce::File getUserPresetDir();
juce::StringArray getUserPresetNames();
bool saveUserPreset (const juce::String& name, const juce::ValueTree& state);
juce::ValueTree loadUserPreset (const juce::String& name);

} // namespace vc

namespace vc
{
// Stable keys keep favorites independent of display names and browser order.
struct RackPresetEntry
{
    enum Source { Init, Factory, Style, User, Studio } source;
    int index { -1 };
    juce::String key, name, legacyName, tags, description;
    int audience { 3 }; // bit 1: recorded singers, bit 2: vocal synthesizers
};
juce::Colour presetCategoryColour (const juce::String&);
juce::Colour presetColour (const RackPresetEntry&);
std::vector<RackPresetEntry> getRackPresetEntries();
juce::ValueTree getRackPresetTree (const RackPresetEntry&);
}
