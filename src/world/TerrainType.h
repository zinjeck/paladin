#pragma once

#include <cstdint>

namespace Paladin
{
    enum class TerrainType : std::uint8_t
    {
        Land,
        Water,
        Mountain
    };
}