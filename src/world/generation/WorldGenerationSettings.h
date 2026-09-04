#pragma once

#include "world/generation/LandmassGenerationTemplate.h"

#include <cstdint>
#include <string>

namespace Paladin
{
    struct WorldGenerationSettings
    {
        std::int32_t width = 600;
        std::int32_t height = 440;

        std::uint64_t seed =
            0x0050'414C'4144'494EULL;

        std::string landmassTemplateId{
            defaultLandmassTemplateId
        };

        // Zero uses the selected template's default. Positive values are
        // optional player or scenario overrides.
        std::int32_t minimumContinentCount = 0;
        std::int32_t maximumContinentCount = 0;

        float seaLevel = 0.46F;
    };
}
