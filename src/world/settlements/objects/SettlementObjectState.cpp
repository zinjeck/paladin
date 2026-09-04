#include "world/settlements/objects/SettlementObjectState.h"

#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace Paladin
{
    namespace
    {
        bool footprintsIntersect(
            const SettlementObjectFootprint& left,
            const SettlementObjectFootprint& right
        ) noexcept
        {
            return
                left.topLeft.x < right.topLeft.x + right.width &&
                left.topLeft.x + left.width > right.topLeft.x &&
                left.topLeft.y < right.topLeft.y + right.height &&
                left.topLeft.y + left.height > right.topLeft.y;
        }
    }


    bool SettlementObjectFootprint::contains(
        WorldTilePosition position
    ) const noexcept
    {
        return
            position.x >= topLeft.x &&
            position.y >= topLeft.y &&
            position.x < topLeft.x + width &&
            position.y < topLeft.y + height;
    }


    SettlementObjectState::SettlementObjectState(
        std::int32_t mapWidth,
        std::int32_t mapHeight
    )
        : mapWidth_(mapWidth),
          mapHeight_(mapHeight),
          structureOccupiedTiles_(
              mapWidth > 0 && mapHeight > 0
                  ? static_cast<std::size_t>(mapWidth)
                    * static_cast<std::size_t>(mapHeight)
                  : 0,
              0
          ),
          infrastructureOccupiedTiles_(
              mapWidth > 0 && mapHeight > 0
                  ? static_cast<std::size_t>(mapWidth)
                    * static_cast<std::size_t>(mapHeight)
                  : 0,
              0
          )
    {
    }


    bool SettlementObjectState::canPlace(
        const WorldGrid& grid,
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    ) const noexcept
    {
        if (
            grid.width() != mapWidth_ ||
            grid.height() != mapHeight_ ||
            !footprintSizeIsAllowed(definition, footprint) ||
            (
                definition.uniquePerSettlement &&
                hasObjectType(definition.id)
            )
        )
        {
            return false;
        }

        const WorldTilePosition bottomRight{
            footprint.topLeft.x + footprint.width - 1,
            footprint.topLeft.y + footprint.height - 1
        };

        if (
            !grid.isValidPosition(footprint.topLeft) ||
            !grid.isValidPosition(bottomRight)
        )
        {
            return false;
        }

        std::size_t buildableTileCount = 0;

        for (
            std::int32_t y = footprint.topLeft.y;
            y <= bottomRight.y;
            ++y
        )
        {
            for (
                std::int32_t x = footprint.topLeft.x;
                x <= bottomRight.x;
                ++x
            )
            {
                const SettlementTilePlacementStatus status =
                    placementStatusAt(grid, definition, {x, y});

                if (status == SettlementTilePlacementStatus::Buildable)
                {
                    ++buildableTileCount;
                }
                else if (!definition.allowsPartialPlacement)
                {
                    return false;
                }
            }
        }

        return buildableTileCount > 0;
    }


    SettlementTilePlacementStatus
    SettlementObjectState::placementStatusAt(
        const WorldGrid& grid,
        const SettlementObjectDefinition& definition,
        WorldTilePosition position
    ) const noexcept
    {
        const WorldTile* tile = grid.tile(position);

        if (
            grid.width() != mapWidth_ ||
            grid.height() != mapHeight_ ||
            !tile ||
            tile->terrain == TerrainType::Water ||
            tile->terrain == TerrainType::Mountain
        )
        {
            return SettlementTilePlacementStatus::InvalidTerrain;
        }

        const std::size_t index = tileIndex(position);

        if (structureOccupiedTiles_[index] != 0)
        {
            return SettlementTilePlacementStatus::Occupied;
        }

        if (
            definition.placementLayer ==
                SettlementObjectPlacementLayer::Infrastructure &&
            infrastructureOccupiedTiles_[index] != 0
        )
        {
            return SettlementTilePlacementStatus::Occupied;
        }

        return SettlementTilePlacementStatus::Buildable;
    }


    bool SettlementObjectState::placeCompletedObject(
        const WorldGrid& grid,
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    )
    {
        if (
            !definition.bypassesConstruction ||
            !canPlace(grid, definition, footprint)
        )
        {
            return false;
        }

        if (
            definition.placementLayer ==
                SettlementObjectPlacementLayer::Structure
        )
        {
            removeInfrastructureWithin(footprint);
        }

        completedObjects_.push_back({
            objectIds_.generate(),
            std::string(definition.id),
            footprint
        });

        occupy(definition, footprint);
        ++presentationVersion_;
        return true;
    }


    bool SettlementObjectState::createConstructionSites(
        const WorldGrid& grid,
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    )
    {
        if (
            definition.bypassesConstruction ||
            !canPlace(grid, definition, footprint)
        )
        {
            return false;
        }

        if (
            definition.placementLayer ==
                SettlementObjectPlacementLayer::Structure
        )
        {
            removeInfrastructureWithin(footprint);
        }

        if (definition.separateConstructionSitePerTile)
        {
            for (
                std::int32_t y = footprint.topLeft.y;
                y < footprint.topLeft.y + footprint.height;
                ++y
            )
            {
                for (
                    std::int32_t x = footprint.topLeft.x;
                    x < footprint.topLeft.x + footprint.width;
                    ++x
                )
                {
                    const WorldTilePosition position{x, y};

                    if (
                        placementStatusAt(
                            grid,
                            definition,
                            position
                        ) != SettlementTilePlacementStatus::Buildable
                    )
                    {
                        continue;
                    }

                    const SettlementObjectFootprint tileFootprint{
                        position,
                        1,
                        1
                    };

                    constructionSites_.push_back({
                        constructionSiteIds_.generate(),
                        std::string(definition.id),
                        tileFootprint
                    });

                    occupy(definition, tileFootprint);
                }
            }
        }
        else
        {
            constructionSites_.push_back({
                constructionSiteIds_.generate(),
                std::string(definition.id),
                footprint
            });

            occupy(definition, footprint);
        }

        ++presentationVersion_;
        return true;
    }


    const std::vector<CompletedSettlementObject>&
    SettlementObjectState::completedObjects() const noexcept
    {
        return completedObjects_;
    }


    const std::vector<SettlementConstructionSite>&
    SettlementObjectState::constructionSites() const noexcept
    {
        return constructionSites_;
    }


    std::uint64_t
    SettlementObjectState::presentationVersion() const noexcept
    {
        return presentationVersion_;
    }


    bool SettlementObjectState::footprintSizeIsAllowed(
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    ) const noexcept
    {
        return
            footprint.width >= definition.minimumWidth &&
            footprint.height >= definition.minimumHeight &&
            footprint.width <= definition.maximumWidth &&
            footprint.height <= definition.maximumHeight &&
            (
                definition.selectionMode !=
                    SettlementFootprintSelectionMode::Fixed ||
                (
                    footprint.width == definition.previewWidth &&
                    footprint.height == definition.previewHeight
                )
            );
    }


    bool SettlementObjectState::hasObjectType(
        std::string_view objectTypeId
    ) const noexcept
    {
        for (const CompletedSettlementObject& object : completedObjects_)
        {
            if (object.objectTypeId == objectTypeId)
            {
                return true;
            }
        }

        for (const SettlementConstructionSite& site : constructionSites_)
        {
            if (site.objectTypeId == objectTypeId)
            {
                return true;
            }
        }

        return false;
    }


    void SettlementObjectState::removeInfrastructureWithin(
        const SettlementObjectFootprint& footprint
    )
    {
        const auto removeCompleted = [this, &footprint](
            const CompletedSettlementObject& object
        )
        {
            const SettlementObjectDefinition* definition =
                SettlementObjectCatalog::definition(object.objectTypeId);

            if (
                !definition ||
                definition->placementLayer !=
                    SettlementObjectPlacementLayer::Infrastructure ||
                !footprintsIntersect(object.footprint, footprint)
            )
            {
                return false;
            }

            clearInfrastructureOccupancy(object.footprint);
            return true;
        };

        const auto removeConstruction = [this, &footprint](
            const SettlementConstructionSite& site
        )
        {
            const SettlementObjectDefinition* definition =
                SettlementObjectCatalog::definition(site.objectTypeId);

            if (
                !definition ||
                definition->placementLayer !=
                    SettlementObjectPlacementLayer::Infrastructure ||
                !footprintsIntersect(site.footprint, footprint)
            )
            {
                return false;
            }

            clearInfrastructureOccupancy(site.footprint);
            return true;
        };

        std::erase_if(completedObjects_, removeCompleted);
        std::erase_if(constructionSites_, removeConstruction);
    }


    void SettlementObjectState::occupy(
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    ) noexcept
    {
        std::vector<std::uint8_t>& occupiedTiles =
            definition.placementLayer ==
                SettlementObjectPlacementLayer::Infrastructure
                ? infrastructureOccupiedTiles_
                : structureOccupiedTiles_;

        for (
            std::int32_t y = footprint.topLeft.y;
            y < footprint.topLeft.y + footprint.height;
            ++y
        )
        {
            for (
                std::int32_t x = footprint.topLeft.x;
                x < footprint.topLeft.x + footprint.width;
                ++x
            )
            {
                occupiedTiles[tileIndex({x, y})] = 1;
            }
        }
    }


    void SettlementObjectState::clearInfrastructureOccupancy(
        const SettlementObjectFootprint& footprint
    ) noexcept
    {
        for (
            std::int32_t y = footprint.topLeft.y;
            y < footprint.topLeft.y + footprint.height;
            ++y
        )
        {
            for (
                std::int32_t x = footprint.topLeft.x;
                x < footprint.topLeft.x + footprint.width;
                ++x
            )
            {
                infrastructureOccupiedTiles_[tileIndex({x, y})] = 0;
            }
        }
    }


    std::size_t SettlementObjectState::tileIndex(
        WorldTilePosition position
    ) const noexcept
    {
        return
            static_cast<std::size_t>(position.y)
                * static_cast<std::size_t>(mapWidth_)
            + static_cast<std::size_t>(position.x);
    }
}
