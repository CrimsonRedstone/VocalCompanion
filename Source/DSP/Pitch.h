#pragma once
#include "SpectralPitch.h"
namespace vc
{
class PitchFormantModule final : public SpectralPitchModule
{
public:
    PitchFormantModule() : SpectralPitchModule (false) {}
};
}
