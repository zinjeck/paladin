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
        BiomeType classifyLandBiome(
            float temperature,
            float rainfall
        ) noexcept
        {
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
                return rainfall < 0.45F
                    ? BiomeType::Tundra
                    : BiomeType::Taiga;
            }

            return rainfall < 0.42F
                ? BiomeType::Plain
                : BiomeType::Forest;
        }
    }

    void TerrainBiomeClassifier::classify(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                WorldTile* tile =
                    grid.tile({x, y});

                if (
                    tile->elevation.value()
                    <= settings.seaLevel
                )
                {
                    tile->terrain = TerrainType::Water;
                    tile->biome = BiomeType::Ocean;
                    continue;
                }

                const double ridgeNoise =
                    1.0 - std::abs(
                        GenerationNoise::fractal(
                            static_cast<double>(x) * 0.017,
                            static_cast<double>(y) * 0.017,
                            settings.seed
                                ^ 0x7351'7D93'2B4A'C861ULL,
                            4,
                            0.57,
                            2.2
                        )
                    );

                const double landElevation =
                    std::clamp(
                        (
                            static_cast<double>(
                                tile->elevation.value()
                            )
                            - settings.seaLevel
                        )
                        / (1.0 - settings.seaLevel),
                        0.0,
                        1.0
                    );

                const double mountainScore =
                    ridgeNoise * 0.82
                    + landElevation * 0.18;

                tile->terrain = mountainScore >= 0.76
                    ? TerrainType::Mountain
                    : TerrainType::Land;

                tile->biome = classifyLandBiome(
                    tile->temperature.value(),
                    tile->rainfall.value()
                );
            }
        }
    }
}
