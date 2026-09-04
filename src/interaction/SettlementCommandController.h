#pragma once

#include "world/settlements/objects/SettlementObjectState.h"

#include <optional>
#include <string>
#include <string_view>

namespace Paladin
{
    class SettlementCitizenState;
    class SettlementMap;

    class SettlementCommandController
    {
    public:
        [[nodiscard]]
        bool begin(std::string_view commandTypeId);

        void cancel() noexcept;

        [[nodiscard]]
        bool isActive() const noexcept;

        [[nodiscard]]
        bool isCancelMode() const noexcept;

        void pointerMoved(
            std::optional<WorldTilePosition> position
        ) noexcept;

        void pointerPressed(
            std::optional<WorldTilePosition> position
        ) noexcept;

        [[nodiscard]]
        bool pointerReleased(
            std::optional<WorldTilePosition> position,
            SettlementMap& settlementMap,
            SettlementCitizenState& citizens
        );

        [[nodiscard]]
        std::optional<SettlementObjectFootprint>
        visibleFootprint() const noexcept;

    private:
        std::string commandTypeId_;
        std::optional<WorldTilePosition> dragStart_;
        std::optional<WorldTilePosition> hoveredPosition_;
    };
}
