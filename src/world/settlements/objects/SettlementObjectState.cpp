#include "world/settlements/objects/SettlementObjectState.h"

#include "world/TerrainType.h"
#include "world/SettlementGrid.h"
#include "world/WorldTile.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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


        std::vector<ConstructionResourceDelivery>
        initialResourceDeliveries(
            const SettlementObjectDefinition& definition
        )
        {
            std::vector<ConstructionResourceDelivery> deliveries;
            deliveries.reserve(
                definition.constructionResourceCosts.size()
            );

            for (const SettlementConstructionResourceCost& cost :
                definition.constructionResourceCosts)
            {
                deliveries.push_back({
                    std::string(cost.resourceId),
                    0,
                    cost.requiredAmount
                });
            }

            return deliveries;
        }
    }


    bool SettlementObjectFootprint::contains(
        SettlementTilePosition position
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
          ),
          structureBlockedPrefix_(
              mapWidth > 0 && mapHeight > 0
                  ? (static_cast<std::size_t>(mapWidth) + 1U)
                    * (static_cast<std::size_t>(mapHeight) + 1U)
                  : 0,
              0
          ),
          infrastructureBlockedPrefix_(
              mapWidth > 0 && mapHeight > 0
                  ? (static_cast<std::size_t>(mapWidth) + 1U)
                    * (static_cast<std::size_t>(mapHeight) + 1U)
                  : 0,
              0
          )
    {
    }


    bool SettlementObjectState::canPlace(
        const SettlementGrid& grid,
        const SettlementObjectDefinition& definition,
        const SettlementObjectFootprint& footprint
    ) const noexcept
    {
        const SettlementPlacementAreaEvaluation evaluation =
            evaluatePlacementArea(grid, definition, footprint);

        return
            evaluation.footprintAllowed &&
            (
                definition.allowsPartialPlacement
                    ? evaluation.buildableTileCount > 0
                    : evaluation.blockedTileCount == 0
            );
    }


    SettlementPlacementAreaEvaluation
    SettlementObjectState::evaluatePlacementArea(
        const SettlementGrid& grid,
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
            return {};
        }

        const SettlementTilePosition bottomRight{
            footprint.topLeft.x + footprint.width - 1,
            footprint.topLeft.y + footprint.height - 1
        };

        if (
            !grid.isValidPosition(footprint.topLeft) ||
            !grid.isValidPosition(bottomRight)
        )
        {
            return {};
        }

        rebuildPlacementPrefixCache(grid);
        const std::vector<std::uint32_t>& blockedPrefix =
            definition.placementLayer ==
                SettlementObjectPlacementLayer::Infrastructure
                ? infrastructureBlockedPrefix_
                : structureBlockedPrefix_;
        const std::size_t blocked = blockedTileCountIn(
            blockedPrefix,
            footprint
        );
        const std::size_t total =
            static_cast<std::size_t>(footprint.width) *
            static_cast<std::size_t>(footprint.height);

        return {total - blocked, blocked, true};
    }


    SettlementTilePlacementStatus
    SettlementObjectState::placementStatusAt(
        const SettlementGrid& grid,
        const SettlementObjectDefinition& definition,
        SettlementTilePosition position
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
        const SettlementGrid& grid,
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
        const SettlementGrid& grid,
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
                std::optional<std::int32_t> runStart;
                for (
                    std::int32_t x = footprint.topLeft.x;
                    x <= footprint.topLeft.x + footprint.width;
                    ++x
                )
                {
                    const SettlementTilePosition position{x, y};
                    const bool buildable =
                        x < footprint.topLeft.x + footprint.width &&
                        placementStatusAt(
                            grid,
                            definition,
                            position
                        ) == SettlementTilePlacementStatus::Buildable;

                    if (buildable)
                    {
                        if (!runStart)
                        {
                            runStart = x;
                        }
                        infrastructureOccupiedTiles_[tileIndex(position)] = 1;
                    }
                    else if (runStart)
                    {
                        constructionSites_.push_back({
                            constructionSiteIds_.generate(),
                            std::string(definition.id),
                            {{*runStart, y}, x - *runStart, 1},
                            ConstructionSitePhase::AwaitingMaterials,
                            0,
                            initialResourceDeliveries(definition)
                        });
                        runStart.reset();
                    }
                }
            }
        }
        else
        {
            constructionSites_.push_back({
                constructionSiteIds_.generate(),
                std::string(definition.id),
                footprint,
                ConstructionSitePhase::AwaitingMaterials,
                0,
                initialResourceDeliveries(definition)
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


    const CompletedSettlementObject*
    SettlementObjectState::completedObject(
        SettlementObjectId id
    ) const noexcept
    {
        const auto iterator = std::find_if(
            completedObjects_.begin(),
            completedObjects_.end(),
            [id](const CompletedSettlementObject& object)
            {
                return object.id == id;
            }
        );

        return iterator == completedObjects_.end()
            ? nullptr
            : &*iterator;
    }


    const SettlementConstructionSite*
    SettlementObjectState::constructionSite(
        ConstructionSiteId id
    ) const noexcept
    {
        const auto iterator = std::find_if(
            constructionSites_.begin(),
            constructionSites_.end(),
            [id](const SettlementConstructionSite& site)
            {
                return site.id == id;
            }
        );

        return iterator == constructionSites_.end()
            ? nullptr
            : &*iterator;
    }


    const CompletedSettlementObject*
    SettlementObjectState::completedObjectAt(
        SettlementTilePosition position
    ) const noexcept
    {
        const auto iterator = std::find_if(
            completedObjects_.rbegin(),
            completedObjects_.rend(),
            [position](const CompletedSettlementObject& object)
            {
                return object.footprint.contains(position);
            }
        );

        return iterator == completedObjects_.rend()
            ? nullptr
            : &*iterator;
    }


    const SettlementConstructionSite*
    SettlementObjectState::constructionSiteAt(
        SettlementTilePosition position
    ) const noexcept
    {
        const auto iterator = std::find_if(
            constructionSites_.rbegin(),
            constructionSites_.rend(),
            [position](const SettlementConstructionSite& site)
            {
                return site.footprint.contains(position);
            }
        );

        return iterator == constructionSites_.rend()
            ? nullptr
            : &*iterator;
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

        std::erase_if(completedObjects_, removeCompleted);

        std::vector<SettlementConstructionSite> retainedSites;
        retainedSites.reserve(constructionSites_.size() + 4U);

        for (const SettlementConstructionSite& site : constructionSites_)
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
                retainedSites.push_back(site);
                continue;
            }

            clearInfrastructureOccupancy(site.footprint);
            const std::int32_t siteRight =
                site.footprint.topLeft.x + site.footprint.width;
            const std::int32_t siteBottom =
                site.footprint.topLeft.y + site.footprint.height;
            const std::int32_t cutLeft = std::max(
                site.footprint.topLeft.x,
                footprint.topLeft.x
            );
            const std::int32_t cutTop = std::max(
                site.footprint.topLeft.y,
                footprint.topLeft.y
            );
            const std::int32_t cutRight = std::min(
                siteRight,
                footprint.topLeft.x + footprint.width
            );
            const std::int32_t cutBottom = std::min(
                siteBottom,
                footprint.topLeft.y + footprint.height
            );
            const std::array<SettlementObjectFootprint, 4> pieces{{
                {
                    site.footprint.topLeft,
                    site.footprint.width,
                    cutTop - site.footprint.topLeft.y
                },
                {
                    {site.footprint.topLeft.x, cutBottom},
                    site.footprint.width,
                    siteBottom - cutBottom
                },
                {
                    {site.footprint.topLeft.x, cutTop},
                    cutLeft - site.footprint.topLeft.x,
                    cutBottom - cutTop
                },
                {
                    {cutRight, cutTop},
                    siteRight - cutRight,
                    cutBottom - cutTop
                }
            }};

            bool reusedId = false;
            for (const SettlementObjectFootprint& piece : pieces)
            {
                if (piece.width <= 0 || piece.height <= 0)
                {
                    continue;
                }

                retainedSites.push_back({
                    reusedId ? constructionSiteIds_.generate() : site.id,
                    site.objectTypeId,
                    piece,
                    site.phase,
                    site.progressPermille,
                    site.resourceDeliveries
                });
                reusedId = true;
                occupy(*definition, piece);
            }
        }

        constructionSites_ = std::move(retainedSites);
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
        SettlementTilePosition position
    ) const noexcept
    {
        return
            static_cast<std::size_t>(position.y)
                * static_cast<std::size_t>(mapWidth_)
            + static_cast<std::size_t>(position.x);
    }


    void SettlementObjectState::rebuildPlacementPrefixCache(
        const SettlementGrid& grid
    ) const noexcept
    {
        if (
            cachedPlacementGrid_ == &grid &&
            cachedPlacementVersion_ == presentationVersion_
        )
        {
            return;
        }

        const std::size_t prefixWidth =
            static_cast<std::size_t>(mapWidth_) + 1U;
        std::fill(
            structureBlockedPrefix_.begin(),
            structureBlockedPrefix_.end(),
            0
        );
        std::fill(
            infrastructureBlockedPrefix_.begin(),
            infrastructureBlockedPrefix_.end(),
            0
        );

        for (std::int32_t y = 0; y < mapHeight_; ++y)
        {
            std::uint32_t structureRow = 0;
            std::uint32_t infrastructureRow = 0;

            for (std::int32_t x = 0; x < mapWidth_; ++x)
            {
                const std::size_t tile = tileIndex({x, y});
                const WorldTile* worldTile = grid.tile({x, y});
                const bool invalidTerrain =
                    !worldTile ||
                    worldTile->terrain == TerrainType::Water ||
                    worldTile->terrain == TerrainType::Mountain;
                const bool structureBlocked =
                    invalidTerrain || structureOccupiedTiles_[tile] != 0;
                const bool infrastructureBlocked =
                    structureBlocked || infrastructureOccupiedTiles_[tile] != 0;

                structureRow += structureBlocked ? 1U : 0U;
                infrastructureRow += infrastructureBlocked ? 1U : 0U;
                const std::size_t prefixIndex =
                    static_cast<std::size_t>(y + 1) * prefixWidth +
                    static_cast<std::size_t>(x + 1);
                structureBlockedPrefix_[prefixIndex] =
                    structureBlockedPrefix_[
                        static_cast<std::size_t>(y) * prefixWidth +
                        static_cast<std::size_t>(x + 1)
                    ] + structureRow;
                infrastructureBlockedPrefix_[prefixIndex] =
                    infrastructureBlockedPrefix_[
                        static_cast<std::size_t>(y) * prefixWidth +
                        static_cast<std::size_t>(x + 1)
                    ] + infrastructureRow;
            }
        }

        cachedPlacementGrid_ = &grid;
        cachedPlacementVersion_ = presentationVersion_;
    }


    std::uint32_t SettlementObjectState::blockedTileCountIn(
        const std::vector<std::uint32_t>& prefix,
        const SettlementObjectFootprint& footprint
    ) const noexcept
    {
        const std::size_t stride = static_cast<std::size_t>(mapWidth_) + 1U;
        const std::size_t left = static_cast<std::size_t>(footprint.topLeft.x);
        const std::size_t top = static_cast<std::size_t>(footprint.topLeft.y);
        const std::size_t right = left + static_cast<std::size_t>(footprint.width);
        const std::size_t bottom = top + static_cast<std::size_t>(footprint.height);

        return
            prefix[bottom * stride + right] -
            prefix[top * stride + right] -
            prefix[bottom * stride + left] +
            prefix[top * stride + left];
    }
}
