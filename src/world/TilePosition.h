#pragma once

#include <compare>
#include <cstdint>

namespace Paladin
{
    template<typename SpaceTag>
    struct TilePosition
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        friend constexpr bool operator==(
            const TilePosition&,
            const TilePosition&
        ) noexcept = default;

        friend constexpr auto operator<=>(
            const TilePosition&,
            const TilePosition&
        ) noexcept = default;
    };
}
