#pragma once
#include "world/BiomeType.h"
#include <algorithm>
#include <cstdint>
namespace Paladin
{
    enum class TreeSpecies
    {
        Broadleaf,
        Birch,
        Conifer
    };
    // Choose exactly one species at an existing tree site. Climate is inherited
    // from the world's latitude/temperature field, never camera state or RNG.
    inline TreeSpecies cityTreeSpecies(
        std::uint64_t seed,
        BiomeType biome,
        float temperature = .5F
    )
    {
        if (temperature <= .34F || biome == BiomeType::Tundra || biome == BiomeType::Polar)
        {
            return TreeSpecies::Conifer;
        }
        const float cold = std::clamp((.65F - temperature) / .31F, 0.F, 1.F);
        const float coniferShare = cold * cold * cold;
        const auto roll = seed % 10000;
        if (roll < static_cast<unsigned>(coniferShare * 10000.F))
        {
            return TreeSpecies::Conifer;
        }
        // Birch stays uncommon among the remaining deciduous trees.
        return (seed / 10000) % 100 < 6 ? TreeSpecies::Birch
                                        : TreeSpecies::Broadleaf;
    }
} // namespace Paladin
