#include "interaction/SettlementPlacementController.h"

#include "world/World.h"

namespace Paladin
{
    void SettlementPlacementController::beginSelection(
        PolityId ownerPolityId
    ) noexcept
    {
        selecting_ = true;
        lockedPosition_.reset();
        ownerPolityId_ = ownerPolityId;
    }

    void SettlementPlacementController::cancelSelection() noexcept
    {
        selecting_ = false;
        hoveredPosition_.reset();
        lockedPosition_.reset();
        ownerPolityId_ = {};
    }

    bool SettlementPlacementController::isSelecting() const noexcept
    {
        return selecting_;
    }

    bool SettlementPlacementController::hasLockedSelection() const noexcept
    {
        return lockedPosition_.has_value();
    }

    bool SettlementPlacementController::isActive() const noexcept
    {
        return selecting_ || hasLockedSelection();
    }

    void SettlementPlacementController::setHoveredPosition(
        std::optional<WorldTilePosition> position
    ) noexcept
    {
        hoveredPosition_ = position;
    }

    std::optional<WorldTilePosition>
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
            world.canFoundSettlementAt(
                *hoveredPosition_,
                ownerPolityId_
            );
    }

    bool SettlementPlacementController::lockHoveredSelection(
        const World& world
    ) noexcept
    {
        if (!hasValidPlacement(world))
        {
            return false;
        }

        lockedPosition_ = hoveredPosition_;
        hoveredPosition_.reset();
        selecting_ = false;
        return true;
    }

    std::optional<WorldTilePosition>
    SettlementPlacementController::lockedPosition() const noexcept
    {
        return lockedPosition_;
    }
}
