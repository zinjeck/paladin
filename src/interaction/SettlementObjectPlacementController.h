#pragma once

#include "world/SettlementTilePosition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <optional>
#include <string>
#include <string_view>

namespace Paladin
{
    class SettlementMap;
    struct SettlementObjectDefinition;

    enum class SettlementPlacementCommitResult
    {
        None,
        CompletedObject,
        ConstructionSites
    };

    class SettlementObjectPlacementController
    {
    public:
        [[nodiscard]]
        bool beginPlacement(std::string_view objectTypeId);

        void cancelPlacement() noexcept;

        [[nodiscard]]
        bool isActive() const noexcept;

        [[nodiscard]]
        bool isDragging() const noexcept;

        [[nodiscard]]
        bool hasLockedFootprint() const noexcept;

        [[nodiscard]]
        const SettlementObjectDefinition* activeDefinition() const noexcept;

        void pointerMoved(
            std::optional<SettlementTilePosition> position
        ) noexcept;

        [[nodiscard]]
        SettlementPlacementCommitResult pointerPressed(
            std::optional<SettlementTilePosition> position,
            SettlementMap& settlementMap
        );

        [[nodiscard]]
        bool pointerReleased(
            std::optional<SettlementTilePosition> position,
            const SettlementMap& settlementMap
        ) noexcept;

        [[nodiscard]]
        std::optional<SettlementObjectFootprint>
        visibleFootprint() const noexcept;

        [[nodiscard]]
        bool visibleFootprintIsValid(
            const SettlementMap& settlementMap
        ) const noexcept;

    private:
        [[nodiscard]]
        std::optional<SettlementObjectFootprint>
        currentFootprint() const noexcept;

        [[nodiscard]]
        SettlementPlacementCommitResult commitFootprint(
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint,
            SettlementMap& settlementMap
        );

        std::string activeObjectTypeId_;
        std::optional<SettlementTilePosition> hoveredPosition_;
        std::optional<SettlementTilePosition> dragStart_;
        std::optional<SettlementObjectFootprint> lockedFootprint_;
        bool dragging_ = false;
    };
}
