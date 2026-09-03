#pragma once

#include <compare>
#include <cstdint>

namespace Paladin
{
    struct WorldTilePosition
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        friend constexpr bool operator==(
            const WorldTilePosition&,
            const WorldTilePosition&
        ) noexcept = default;

        friend constexpr auto operator<=>(
            const WorldTilePosition&,
            const WorldTilePosition&
        ) noexcept = default;
    };
}