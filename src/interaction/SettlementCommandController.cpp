#include "interaction/SettlementCommandController.h"

#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"

#include <algorithm>

namespace Paladin
{
    bool SettlementCommandController::begin(
        std::string_view commandTypeId
    )
    {
        if (
            commandTypeId != SettlementCommandTypes::Cancel &&
            !SettlementCommandCatalog::definition(commandTypeId)
        )
        {
            return false;
        }

        commandTypeId_ = commandTypeId;
        dragStart_.reset();
        hoveredPosition_.reset();
        return true;
    }


    void SettlementCommandController::cancel() noexcept
    {
        commandTypeId_.clear();
        dragStart_.reset();
        hoveredPosition_.reset();
    }


    bool SettlementCommandController::isActive() const noexcept
    {
        return !commandTypeId_.empty();
    }


    bool SettlementCommandController::isCancelMode() const noexcept
    {
        return commandTypeId_ == SettlementCommandTypes::Cancel;
    }


    void SettlementCommandController::pointerMoved(
        std::optional<SettlementTilePosition> position
    ) noexcept
    {
        if (isActive())
        {
            hoveredPosition_ = position;
        }
    }


    void SettlementCommandController::pointerPressed(
        std::optional<SettlementTilePosition> position
    ) noexcept
    {
        if (!isActive() || !position)
        {
            return;
        }

        dragStart_ = position;
        hoveredPosition_ = position;
    }


    bool SettlementCommandController::pointerReleased(
        std::optional<SettlementTilePosition> position,
        SettlementMap& settlementMap,
        SettlementCitizenState& citizens
    )
    {
        if (!isActive() || !dragStart_)
        {
            return false;
        }

        if (position)
        {
            hoveredPosition_ = position;
        }

        const std::optional<SettlementObjectFootprint> footprint =
            visibleFootprint();
        dragStart_.reset();

        if (!footprint)
        {
            return false;
        }

        if (isCancelMode())
        {
            return settlementMap.commandState().cancelIntersecting(
                settlementMap,
                *footprint,
                citizens
            ) > 0;
        }

        return settlementMap.commandState().add(
            settlementMap,
            commandTypeId_,
            *footprint,
            citizens
        );
    }


    std::optional<SettlementObjectFootprint>
    SettlementCommandController::visibleFootprint() const noexcept
    {
        if (!isActive() || !hoveredPosition_)
        {
            return std::nullopt;
        }

        const SettlementTilePosition start = dragStart_.value_or(
            *hoveredPosition_
        );
        const std::int32_t left = std::min(start.x, hoveredPosition_->x);
        const std::int32_t top = std::min(start.y, hoveredPosition_->y);
        const std::int32_t right = std::max(start.x, hoveredPosition_->x);
        const std::int32_t bottom = std::max(start.y, hoveredPosition_->y);

        return SettlementObjectFootprint{
            {left, top},
            right - left + 1,
            bottom - top + 1
        };
    }
}
