#pragma once
#include "SpectralPitch.h"
namespace vc
{
class AutoTuneModule final : public SpectralPitchModule
{
public:
    AutoTuneModule() : SpectralPitchModule (true) {}
};
}
