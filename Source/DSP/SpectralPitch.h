#pragma once
#include "Common.h"
namespace vc
{
class SpectralPitchModule : public VcModule
{
public:
    explicit SpectralPitchModule (bool tuner);
    ~SpectralPitchModule() override;
    ModuleType getType() const override { return tuning ? ModuleType::AutoTune : ModuleType::PitchFormant; }
    void prepare (double, int, int) override;
    void reset() override;
    void process (juce::AudioBuffer<float>&) override;
    const std::vector<ParamDesc>& getCoreParamDescs() const override;
    float getCoreParam (int) const override;
    void setCoreParam (int, float) override;
    int latencySamples() const override;
    float getMeter (int = 0) const override { return hz.load(); }
    int getNumMeters() const override { return 1; }
    int copyPitchTrace (float*, float*, int) const override;
private:
    struct Engine;
    std::unique_ptr<Engine> engine;
    bool tuning;
    std::array<std::atomic<float>,6> parameters;
    std::atomic<float> hz { 0 };
    std::array<std::atomic<float>,96> traceIn {}, traceOut {};
    std::atomic<int> traceWrite { 0 };
};
}
