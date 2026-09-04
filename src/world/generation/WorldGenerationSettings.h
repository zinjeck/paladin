#pragma once

#include <cstdint>

namespace Paladin
{
    struct WorldGenerationSettings
    {
        std::int32_t width = 600;
        std::int32_t height = 440;

        std::uint64_t seed =
            0x0050'414C'4144'494EULL;

        std::int32_t minimumContinentCount = 3;
        std::int32_t maximumContinentCount = 5;

        float seaLevel = 0.46F;
    };
}
