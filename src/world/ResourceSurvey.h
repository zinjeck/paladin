#pragma once

#include "world/WorldGrid.h"
#include "world/settlements/SettlementNaturalFeatures.h"
#include <array>
#include <cmath>
#include <string_view>

namespace Paladin
{
    struct ResourcePotential
    {
        std::string_view resource;
        double amount = 0;
        int abundance(double tiles) const
        {
            return amount > 0
                       ? std::clamp(
                             1 + int(std::sqrt(amount / std::max(1., tiles)) *
                                     5),
                             1,
                             5
                         )
                       : 0;
        }
    };
    struct ResourceSurvey
    {
        std::array<ResourcePotential, 8> resources{
            {{"lumber"},
             {"stone"},
             {"wheat"},
             {"fish"},
             {"meat"},
             {"coal"},
             {"iron"},
             {"gold"}}
        };
        double tiles = 0, land = 0;
        double amount(std::string_view resource) const
        {
            for (const auto& entry : resources)
            {
                if (entry.resource == resource)
                {
                    return entry.amount;
                }
            }
            return 0;
        }
    };
    // A survey covers exactly the source rectangle used to generate a city.
    // Natural-resource potential shares the local generation biome policy;
    // refined goods cannot appear as deposits. Call only when the region
    // changes.
    inline ResourceSurvey surveyResources(
        const WorldGrid& grid,
        WorldTilePosition center,
        int width,
        int height
    )
    {
        ResourceSurvey result;
        const NaturalFeatureGenerationPolicy nature;
        for (int y = center.y - height / 2; y < center.y - height / 2 + height;
             ++y)
        {
            for (int x = center.x - width / 2; x < center.x - width / 2 + width;
                 ++x)
            {
                const auto* tile = grid.tile({x, y});
                if (!tile)
                {
                    continue;
                }
                ++result.tiles;
                if (tile->terrain == TerrainType::Water)
                {
                    result.resources[3].amount += 1;
                    continue;
                }
                ++result.land;
                if (tile->terrain != TerrainType::Land)
                {
                    continue;
                }
                for (const auto& biome : nature.biomes)
                {
                    if (biome.biome == tile->biome)
                    {
                        result.resources[0].amount += biome.treeChance;
                        result.resources[1].amount +=
                            biome.rockChance + biome.rockClusterChance;
                        break;
                    }
                }
                if (tile->terrain == TerrainType::Land &&
                    (tile->biome == BiomeType::Plain ||
                     tile->biome == BiomeType::Forest ||
                     tile->biome == BiomeType::Hills) &&
                    tile->temperature.value() > .3 &&
                    tile->temperature.value() < .8)
                {
                    result.resources[2].amount += 1;
                }
                if (tile->terrain == TerrainType::Land &&
                    tile->biome != BiomeType::Polar)
                {
                    result.resources[4].amount += .25;
                }
                if (tile->mineral != MineralDeposit::None)
                {
                    result.resources[4 + std::size_t(tile->mineral)].amount +=
                        1;
                }
            }
        }
        return result;
    }
} // namespace Paladin
