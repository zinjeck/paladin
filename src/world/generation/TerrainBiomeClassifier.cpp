#include "world/generation/TerrainBiomeClassifier.h"

#include "world/BiomeType.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/generation/GenerationNoise.h"
#include "world/generation/WorldGenerationSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Paladin
{
    namespace
    {
        BiomeType classifyLandBiome(float temperature, float rainfall) noexcept
        {
            if (temperature <= .16F)
            {
                return BiomeType::Polar;
            }
            if (temperature >= 0.62F)
            {
                if (rainfall < 0.24F)
                {
                    return BiomeType::Desert;
                }

                if (rainfall < 0.68F)
                {
                    return BiomeType::Plain;
                }

                return BiomeType::Jungle;
            }

            if (temperature <= 0.34F)
            {
                return rainfall < 0.45F ? BiomeType::Tundra : BiomeType::Taiga;
            }

            return rainfall < 0.42F ? BiomeType::Plain : BiomeType::Forest;
        }
    } // namespace

    void TerrainBiomeClassifier::classify(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                WorldTile* tile = grid.tile({x, y});

                if (tile->elevation.value() <= settings.seaLevel)
                {
                    tile->terrain = TerrainType::Water;
                    tile->biome = BiomeType::Ocean;
                    continue;
                }

                const double landElevation = std::clamp(
                    (static_cast<double>(tile->elevation.value()) -
                     settings.seaLevel) /
                        (1.0 - settings.seaLevel),
                    0.0,
                    1.0
                );

                tile->terrain = landElevation >= .52 ? TerrainType::Mountain
                                                     : TerrainType::Land;

                tile->biome = classifyLandBiome(
                    tile->temperature.value(),
                    tile->rainfall.value()
                );
                // Foothills are usable land, with their own city vegetation
                // and stone distribution. Height has no movement penalty.
                if (tile->terrain == TerrainType::Land &&
                    landElevation >= .25 && tile->biome != BiomeType::Polar &&
                    tile->biome != BiomeType::Tundra)
                {
                    tile->biome = BiomeType::Hills;
                }
            }
        }
        // Even narrow ridges must descend through foothills before lowland.
        // Read elevation rather than classifications so traversal order cannot
        // change the generated result.
        for (int y = 0; y < grid.height(); ++y)
        {
            for (int x = 0; x < grid.width(); ++x)
            {
                auto* tile = grid.tile({x, y});
                if (tile->terrain != TerrainType::Land ||
                    tile->biome == BiomeType::Polar ||
                    tile->biome == BiomeType::Tundra)
                {
                    continue;
                }
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const auto* neighbor = grid.tile({x + dx, y + dy});
                        if (neighbor && neighbor->elevation.value() >=
                                            settings.seaLevel +
                                                (1 - settings.seaLevel) * .52)
                        {
                            tile->biome = BiomeType::Hills;
                        }
                    }
                }
            }
        }
    }
} // namespace Paladin
