#include "world/settlements/objects/SettlementObjectState.h"

#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace Paladin
{
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
          occupiedTiles_(
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
                const WorldTilePosition position{x, y};
                const WorldTile* tile = grid.tile(position);

                if (
                    !tile ||
                    tile->terrain == TerrainType::Water ||
                    tile->terrain == TerrainType::Mountain ||
                    occupiedTiles_[tileIndex(position)] != 0
                )
                {
                    return false;
                }
            }
        }

        return true;
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

        completedObjects_.push_back({
            objectIds_.generate(),
            std::string(definition.id),
            footprint
        });

        occupy(footprint);
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
                    constructionSites_.push_back({
                        constructionSiteIds_.generate(),
                        std::string(definition.id),
                        {{x, y}, 1, 1}
                    });
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
        }

        occupy(footprint);
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


    void SettlementObjectState::occupy(
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
                occupiedTiles_[tileIndex({x, y})] = 1;
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
