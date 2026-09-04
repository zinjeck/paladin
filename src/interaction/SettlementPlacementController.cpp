#include "interaction/SettlementPlacementController.h"

#include "world/World.h"

namespace Paladin
{
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
            hoveredPosition_.has_value() &&
            world.canFoundSettlementAt(*hoveredPosition_);
    }

    SettlementId SettlementPlacementController::tryFoundSettlement(
        World& world,
        PolityId ownerPolityId
    )
    {
        if (!hoveredPosition_)
        {
            return {};
        }

        return world.foundSettlement(
            *hoveredPosition_,
            ownerPolityId
        );
    }
}
