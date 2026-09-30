#pragma once
#include "DSP/Common.h"
#include "DSP/Gain.h"
#include "DSP/Dynamics.h"
#include "DSP/EqImager.h"
#include "DSP/Pitch.h"
#include "DSP/AutoTune.h"
#include "DSP/Modulation.h"
#include "DSP/Space.h"

#include "DSP/FxExtras.h"
#include "DSP/Cleanup.h"
#include "DSP/WaveShaper.h"
#include "DSP/Drive.h"

namespace vc
{

class ExternalModule; // Host/ExternalHost.h

std::unique_ptr<VcModule> createModule (ModuleType type);

inline const ModuleType kPalette[] = {
    ModuleType::Gain, ModuleType::DeEsser, ModuleType::FetComp, ModuleType::OptoComp,
    ModuleType::Limiter, ModuleType::DynamicEq, ModuleType::ParaEq, ModuleType::PitchFormant,
    ModuleType::AutoTune,
    ModuleType::Imager, ModuleType::MsEq, ModuleType::AirBreath, ModuleType::Exciter,
    ModuleType::RingMod, ModuleType::Bitcrush,
    ModuleType::Chorus, ModuleType::Phaser, ModuleType::Tremolo, ModuleType::AutoPan,
    ModuleType::Delay, ModuleType::Reverb,
    ModuleType::BreathControl, ModuleType::VocalRider, ModuleType::PlosiveControl,
    ModuleType::WaveShaper, ModuleType::TubeOverdrive, ModuleType::RatDistortion
};

inline constexpr int kPaletteCount = 27;

class Chain
{
public:
    void prepare (double sr, int block, int chs);
    void process (juce::AudioBuffer<float>& buffer, double bpm);
    void process (juce::AudioBuffer<float>& buffer, double bpm, const juce::MidiBuffer&, int offset = 0);
    void clearMidi();
    void reset();

    int size() const { return (int) slots.size(); }
    VcModule* get (int i) { return i >= 0 && i < size() ? slots[(size_t) i].get() : nullptr; }
    const VcModule* get (int i) const { return i >= 0 && i < size() ? slots[(size_t) i].get() : nullptr; }

    int add (ModuleType type, int insertAt = -1);
    void replace (int index, ModuleType type);
    int duplicate(int index);
    void remove (int index);
    void move (int from, int to);
    void clear();

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    int latencySamples() const;

private:
    std::vector<std::unique_ptr<VcModule>> slots;
    double sampleRate { 44100.0 };
    int blockSize { 512 };
    int numCh { 2 };
    bool prepared { false };
};

} // namespace vc
