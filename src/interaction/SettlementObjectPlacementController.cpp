#include "interaction/SettlementObjectPlacementController.h"
#include "world/settlements/objects/SettlementDoor.h"

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
        selectedDoor_.reset();
        doorSide_ = 2;
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

    bool SettlementObjectPlacementController::
        hasLockedFootprint() const noexcept
    {
        return lockedFootprint_.has_value();
    }

    const SettlementObjectDefinition* SettlementObjectPlacementController::
        activeDefinition() const noexcept
    {
        return SettlementObjectCatalog::definition(activeObjectTypeId_);
    }

    void SettlementObjectPlacementController::pointerMoved(
        std::optional<SettlementTilePosition> position
    ) noexcept
    {
        if (!isActive())
        {
            return;
        }

        hoveredPosition_ = position;
    }

    SettlementPlacementCommitResult SettlementObjectPlacementController::
        pointerPressed(
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
            if (choosingDoor())
            {
                if (position && doorTileIsValid(*position, settlementMap))
                {
                    const auto outside =
                        outsideDoor(*lockedFootprint_, *position);
                    const auto* tile = settlementMap.grid().tile(outside);
                    if (tile && tile->terrain != TerrainType::Water &&
                        tile->terrain != TerrainType::Mountain &&
                        !settlementMap.objectState().blocksMovement(outside))
                    {
                        selectedDoor_ = position;
                    }
                }
                return SettlementPlacementCommitResult::None;
            }
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

        if (definition->selectionMode ==
            SettlementFootprintSelectionMode::Fixed)
        {
            const std::optional<SettlementObjectFootprint> footprint =
                currentFootprint();

            return footprint
                       ? commitFootprint(*definition, *footprint, settlementMap)
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

        if (!footprint || !definition ||
            !settlementMap.objectState()
                 .canPlace(settlementMap.grid(), *definition, *footprint))
        {
            return false;
        }

        if (definition->hasDoor && footprint->width < 3 &&
            footprint->height < 3)
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

        if (definition && footprint && definition->hasDoor)
        {
            if (footprint->width < 3 && footprint->height < 3)
            {
                return false;
            }
            if (const auto door = visibleDoor())
            {
                const auto outside = outsideDoor(*footprint, *door);
                const auto* tile = settlementMap.grid().tile(outside);
                if (!validDoorTile(*footprint, *door) || !tile ||
                    tile->terrain == TerrainType::Water ||
                    tile->terrain == TerrainType::Mountain ||
                    settlementMap.objectState().blocksMovement(outside))
                {
                    return false;
                }
            }
        }
        return definition &&
               (settlementMap.logistics.founded() ||
                settlementMap.objectState().hasCityKeep() ||
                definition->id == SettlementObjectTypes::CityKeep) &&
               footprint &&
               settlementMap.objectState()
                   .canPlace(settlementMap.grid(), *definition, *footprint);
    }

    std::optional<SettlementObjectFootprint>
    SettlementObjectPlacementController::currentFootprint() const noexcept
    {
        const SettlementObjectDefinition* definition = activeDefinition();

        if (!definition || !hoveredPosition_)
        {
            return std::nullopt;
        }

        if (definition->selectionMode ==
                SettlementFootprintSelectionMode::Fixed ||
            !dragging_ || !dragStart_)
        {
            return SettlementObjectFootprint{
                {hoveredPosition_->x - definition->previewWidth / 2,
                 hoveredPosition_->y - definition->previewHeight / 2},
                definition->previewWidth,
                definition->previewHeight
            };
        }

        const std::int32_t left = std::min(dragStart_->x, hoveredPosition_->x);

        const std::int32_t top = std::min(dragStart_->y, hoveredPosition_->y);

        const std::int32_t right = std::max(dragStart_->x, hoveredPosition_->x);

        const std::int32_t bottom =
            std::max(dragStart_->y, hoveredPosition_->y);

        return SettlementObjectFootprint{
            {left, top},
            right - left + 1,
            bottom - top + 1
        };
    }

    SettlementPlacementCommitResult SettlementObjectPlacementController::
        commitFootprint(
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint,
            SettlementMap& settlementMap
        )
    {
        SettlementObjectState& state = settlementMap.objectState();
        const auto door = visibleDoor();
        if (definition.hasDoor)
        {
            if (!door || !validDoorTile(footprint, *door))
            {
                return SettlementPlacementCommitResult::None;
            }
            const auto outside = outsideDoor(footprint, *door);
            const auto* tile = settlementMap.grid().tile(outside);
            if (!tile || tile->terrain == TerrainType::Water ||
                tile->terrain == TerrainType::Mountain ||
                state.blocksMovement(outside))
            {
                return SettlementPlacementCommitResult::None;
            }
        }
        if (!settlementMap.logistics.founded() && !state.hasCityKeep() &&
            definition.id != SettlementObjectTypes::CityKeep)
        {
            return SettlementPlacementCommitResult::None;
        }
        SettlementPlacementCommitResult result =
            SettlementPlacementCommitResult::None;

        if (definition.bypassesConstruction)
        {
            if (state.placeCompletedObject(
                    settlementMap.grid(),
                    definition,
                    footprint,
                    door
                ))
            {
                result = SettlementPlacementCommitResult::CompletedObject;
            }
        }
        else if (
            state.createConstructionSites(
                settlementMap.grid(),
                definition,
                footprint,
                door
            )
        )
        {
            result = SettlementPlacementCommitResult::ConstructionSites;
        }

        if (result != SettlementPlacementCommitResult::None)
        {
            if (definition.id == SettlementObjectTypes::CityKeep)
            {
                settlementMap.naturalFeatures().clear(footprint);
            }
            settlementMap.logistics.synchronize(state, 0);
            dragging_ = false;
            dragStart_.reset();
            lockedFootprint_.reset();
            selectedDoor_.reset();
            // Keep the selected object/tool ready for another placement.
            if (definition.uniquePerSettlement)
            {
                cancelPlacement();
            }
        }

        return result;
    }
    bool SettlementObjectPlacementController::choosingDoor() const noexcept
    {
        const auto* d = activeDefinition();
        return lockedFootprint_ && d && d->hasDoor && !selectedDoor_;
    }
    void SettlementObjectPlacementController::rotateDoor(int direction) noexcept
    {
        const auto* d = activeDefinition();
        if (d && d->selectionMode == SettlementFootprintSelectionMode::Fixed)
        {
            doorSide_ = (doorSide_ + direction + 4) % 4;
        }
    }
    std::optional<SettlementTilePosition> SettlementObjectPlacementController::
        visibleDoor() const noexcept
    {
        const auto* d = activeDefinition();
        const auto f = visibleFootprint();
        if (!d || !f || !d->hasDoor)
        {
            return std::nullopt;
        }
        if (d->selectionMode == SettlementFootprintSelectionMode::Fixed)
        {
            return centerDoor(*f, doorSide_);
        }
        return selectedDoor_;
    }
    bool SettlementObjectPlacementController::stepBack() noexcept
    {
        if (!isActive())
        {
            return false;
        }
        if (selectedDoor_)
        {
            selectedDoor_.reset();
        }
        else if (lockedFootprint_)
        {
            lockedFootprint_.reset();
        }
        else if (dragging_)
        {
            dragging_ = false;
            dragStart_.reset();
        }
        else
        {
            cancelPlacement();
        }
        return true;
    }
    bool SettlementObjectPlacementController::doorTileIsValid(
        SettlementTilePosition p,
        const SettlementMap& map
    ) const noexcept
    {
        const auto f = visibleFootprint();
        if (!f || !validDoorTile(*f, p))
        {
            return false;
        }
        const auto outside = outsideDoor(*f, p);
        const auto* tile = map.grid().tile(outside);
        return tile && tile->terrain != TerrainType::Water &&
               tile->terrain != TerrainType::Mountain &&
               !map.objectState().blocksMovement(outside) &&
               !map.objectState().constructionSiteAt(outside);
    }
} // namespace Paladin
