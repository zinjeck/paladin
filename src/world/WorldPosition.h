#pragma once

#include <compare>
#include <cstdint>

namespace Paladin
{
    struct WorldPosition
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        friend constexpr bool operator==(
            const WorldPosition&,
            const WorldPosition&
        ) noexcept = default;

        friend constexpr auto operator<=>(
            const WorldPosition&,
            const WorldPosition&
        ) noexcept = default;
    };
}