#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace Paladin
{
    enum class SettlementKind : std::uint8_t
    {
        City,
        Fortress
    };

    constexpr std::string_view settlementKindName(SettlementKind kind) noexcept
    {
        return kind == SettlementKind::Fortress ? "Fortress" : "City";
    }

    // Dimensions, not area: the default 9x9 city region becomes 3x3.
    constexpr int settlementRegionDimension(
        int cityDimension,
        SettlementKind kind
    ) noexcept
    {
        return kind == SettlementKind::Fortress ? std::max(1, cityDimension / 3)
                                                : cityDimension;
    }
} // namespace Paladin
