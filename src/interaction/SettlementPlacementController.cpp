#include "interaction/SettlementPlacementController.h"

#include "world/World.h"

namespace Paladin
{
    void SettlementPlacementController::beginSelection() noexcept
    {
        selecting_ = true;
    }

    void SettlementPlacementController::cancelSelection() noexcept
    {
        selecting_ = false;
        hoveredPosition_.reset();
    }

    bool SettlementPlacementController::isSelecting() const noexcept
    {
        return selecting_;
    }

    void SettlementPlacementController::setHoveredPosition(
        std::optional<WorldPosition> position
    ) noexcept
    {
        hoveredPosition_ = position;
    }

    std::optional<WorldPosition>
    SettlementPlacementController::hoveredPosition() const noexcept
    {
        return hoveredPosition_;
    }

    bool SettlementPlacementController::hasValidPlacement(
        const World& world
    ) const noexcept
    {
        return
            selecting_ &&
            hoveredPosition_.has_value() &&
            world.canFoundSettlementAt(*hoveredPosition_);
    }

    SettlementId SettlementPlacementController::tryFoundSettlement(
        World& world,
        PolityId ownerPolityId
    )
    {
        if (!selecting_ || !hoveredPosition_)
        {
            return {};
        }

        const SettlementId settlementId =
            world.foundSettlement(
                *hoveredPosition_,
                ownerPolityId
            );

        if (settlementId.isValid())
        {
            cancelSelection();
        }

        return settlementId;
    }
}
