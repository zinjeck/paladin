#pragma once

#include <cstdint>
#include <string_view>

namespace Paladin
{
    enum class BiomeType : std::uint8_t
    {
        Plain,
        Forest,
        Jungle,
        Desert,
        Tundra,
        Taiga,
        Ocean,
        Hills,
        Polar
    };
    constexpr std::string_view biomeName(BiomeType biome) noexcept
    {
        switch (biome)
        {
        case BiomeType::Plain:
            return "Plain";
        case BiomeType::Forest:
            return "Forest";
        case BiomeType::Jungle:
            return "Jungle";
        case BiomeType::Desert:
            return "Desert";
        case BiomeType::Tundra:
            return "Tundra";
        case BiomeType::Taiga:
            return "Taiga";
        case BiomeType::Ocean:
            return "Ocean";
        case BiomeType::Hills:
            return "Hills";
        case BiomeType::Polar:
            return "Polar";
        }
        return "Unknown";
    }
} // namespace Paladin
