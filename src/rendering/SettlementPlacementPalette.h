#pragma once

#include "rendering/Renderer.h"

#include <cstdint>

namespace Paladin
{
    enum class SettlementPlacementVisualState : std::uint8_t
    {
        Valid,
        Invalid,
        AwaitingMaterials,
        ReadyToBuild
    };

    [[nodiscard]]
    constexpr RenderColor settlementPlacementOutlineColor(
        SettlementPlacementVisualState state
    ) noexcept
    {
        switch (state)
        {
        case SettlementPlacementVisualState::Invalid:
            return {232, 70, 70, 255};
        case SettlementPlacementVisualState::ReadyToBuild:
            return {55, 135, 225, 255};
        case SettlementPlacementVisualState::Valid:
        case SettlementPlacementVisualState::AwaitingMaterials:
        default:
            return {72, 220, 112, 255};
        }
    }

    [[nodiscard]]
    constexpr RenderColor settlementPlacementFillColor(
        SettlementPlacementVisualState state
    ) noexcept
    {
        RenderColor color = settlementPlacementOutlineColor(state);
        color.alpha = 145;
        return color;
    }
}
