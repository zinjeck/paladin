#include "interaction/SettlementObjectPlacementController.h"

#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <algorithm>

namespace Paladin
{
    bool SettlementObjectPlacementController::beginPlacement(
        std::string_view objectTypeId
    )
    {
        if (!SettlementObjectCatalog::definition(objectTypeId))
        {
            return false;
        }

        activeObjectTypeId_ = objectTypeId;
        hoveredPosition_.reset();
        dragStart_.reset();
        lockedFootprint_.reset();
        dragging_ = false;
        return true;
    }


    void SettlementObjectPlacementController::cancelPlacement() noexcept
    {
        activeObjectTypeId_.clear();
        hoveredPosition_.reset();
        dragStart_.reset();
        lockedFootprint_.reset();
        dragging_ = false;
    }


    bool SettlementObjectPlacementController::isActive() const noexcept
    {
        return !activeObjectTypeId_.empty();
    }


    bool SettlementObjectPlacementController::isDragging() const noexcept
    {
        return dragging_;
    }


    bool SettlementObjectPlacementController::hasLockedFootprint() const noexcept
    {
        return lockedFootprint_.has_value();
    }


    const SettlementObjectDefinition*
    SettlementObjectPlacementController::activeDefinition() const noexcept
    {
        return SettlementObjectCatalog::definition(activeObjectTypeId_);
    }


    void SettlementObjectPlacementController::pointerMoved(
        std::optional<SettlementTilePosition> position
    ) noexcept
    {
        if (!isActive() || lockedFootprint_)
        {
            return;
        }

        hoveredPosition_ = position;
    }


    SettlementPlacementCommitResult
    SettlementObjectPlacementController::pointerPressed(
        std::optional<SettlementTilePosition> position,
        SettlementMap& settlementMap
    )
    {
        const SettlementObjectDefinition* definition = activeDefinition();

        if (!definition)
        {
            return SettlementPlacementCommitResult::None;
        }

        if (lockedFootprint_)
        {
            return commitFootprint(
                *definition,
                *lockedFootprint_,
                settlementMap
            );
        }

        if (!position)
        {
            return SettlementPlacementCommitResult::None;
        }

        hoveredPosition_ = position;

        if (
            definition->selectionMode ==
                SettlementFootprintSelectionMode::Fixed
        )
        {
            const std::optional<SettlementObjectFootprint> footprint =
                currentFootprint();

            return footprint
                ? commitFootprint(
                    *definition,
                    *footprint,
                    settlementMap
                )
                : SettlementPlacementCommitResult::None;
        }

        dragStart_ = position;
        dragging_ = true;
        return SettlementPlacementCommitResult::None;
    }


    bool SettlementObjectPlacementController::pointerReleased(
        std::optional<SettlementTilePosition> position,
        const SettlementMap& settlementMap
    ) noexcept
    {
        if (!isActive() || !dragging_ || lockedFootprint_)
        {
            return false;
        }

        if (position)
        {
            hoveredPosition_ = position;
        }

        const std::optional<SettlementObjectFootprint> footprint =
            currentFootprint();

        dragging_ = false;
        dragStart_.reset();

        const SettlementObjectDefinition* definition = activeDefinition();

        if (
            !footprint ||
            !definition ||
            !settlementMap.objectState().canPlace(
                settlementMap.grid(),
                *definition,
                *footprint
            )
        )
        {
            return false;
        }

        lockedFootprint_ = footprint;
        return true;
    }


    std::optional<SettlementObjectFootprint>
    SettlementObjectPlacementController::visibleFootprint() const noexcept
    {
        return lockedFootprint_ ? lockedFootprint_ : currentFootprint();
    }


    bool SettlementObjectPlacementController::visibleFootprintIsValid(
        const SettlementMap& settlementMap
    ) const noexcept
    {
        const SettlementObjectDefinition* definition = activeDefinition();
        const std::optional<SettlementObjectFootprint> footprint =
            visibleFootprint();

        return
            definition &&
            footprint &&
            settlementMap.objectState().canPlace(
                settlementMap.grid(),
                *definition,
                *footprint
            );
    }


    std::optional<SettlementObjectFootprint>
    SettlementObjectPlacementController::currentFootprint() const noexcept
    {
        const SettlementObjectDefinition* definition = activeDefinition();

        if (!definition || !hoveredPosition_)
        {
            return std::nullopt;
        }

        if (
            definition->selectionMode ==
                SettlementFootprintSelectionMode::Fixed ||
            !dragging_ ||
            !dragStart_
        )
        {
            return SettlementObjectFootprint{
                {
                    hoveredPosition_->x - definition->previewWidth / 2,
                    hoveredPosition_->y - definition->previewHeight / 2
                },
                definition->previewWidth,
                definition->previewHeight
            };
        }

        const std::int32_t left = std::min(
            dragStart_->x,
            hoveredPosition_->x
        );

        const std::int32_t top = std::min(
            dragStart_->y,
            hoveredPosition_->y
        );

        const std::int32_t right = std::max(
            dragStart_->x,
            hoveredPosition_->x
        );

        const std::int32_t bottom = std::max(
            dragStart_->y,
            hoveredPosition_->y
        );

        return SettlementObjectFootprint{
            {left, top},
            right - left + 1,
            bottom - top + 1
        };
    }


    SettlementPlacementCommitResult
    SettlementObjectPlacementController::commitFootprint(
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint,
        SettlementMap& settlementMap
    )
    {
        SettlementObjectState& state = settlementMap.objectState();
        SettlementPlacementCommitResult result =
            SettlementPlacementCommitResult::None;

        if (definition.bypassesConstruction)
        {
            if (state.placeCompletedObject(
                settlementMap.grid(),
                definition,
                footprint
            ))
            {
                result = SettlementPlacementCommitResult::CompletedObject;
            }
        }
        else if (state.createConstructionSites(
            settlementMap.grid(),
            definition,
            footprint
        ))
        {
            result = SettlementPlacementCommitResult::ConstructionSites;
        }

        if (result != SettlementPlacementCommitResult::None)
        {
            cancelPlacement();
        }

        return result;
    }
}
