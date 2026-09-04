#include "world/generation/LandmassGenerator.h"

#include "world/BiomeType.h"
#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/WorldTilePosition.h"
#include "world/generation/GenerationNoise.h"
#include "world/generation/WorldGenerationSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Paladin
{
    namespace
    {
        struct ContinentShape
        {
            double centerX = 0.5;
            double centerY = 0.5;
            double radiusX = 0.15;
            double radiusY = 0.18;
            double rotationRadians = 0.0;
            std::uint64_t noiseSeed = 0;
        };

        using Point = std::array<double, 2>;

        std::array<Point, 5> layoutForCount(
            std::int32_t count
        ) noexcept
        {
            if (count == 3)
            {
                return {{
                    {0.24, 0.30},
                    {0.76, 0.29},
                    {0.50, 0.73},
                    {0.50, 0.50},
                    {0.50, 0.50}
                }};
            }

            if (count == 4)
            {
                return {{
                    {0.25, 0.26},
                    {0.75, 0.26},
                    {0.25, 0.73},
                    {0.75, 0.73},
                    {0.50, 0.50}
                }};
            }

            return {{
                {0.50, 0.16},
                {0.79, 0.38},
                {0.68, 0.75},
                {0.32, 0.75},
                {0.21, 0.38}
            }};
        }

        std::vector<ContinentShape> createContinents(
            const WorldGenerationSettings& settings
        )
        {
            const std::int32_t countRange =
                settings.maximumContinentCount
                - settings.minimumContinentCount
                + 1;

            const std::int32_t count =
                settings.minimumContinentCount
                + static_cast<std::int32_t>(
                    GenerationNoise::mix(settings.seed)
                    % static_cast<std::uint64_t>(countRange)
                );

            const std::array<Point, 5> layout =
                layoutForCount(count);

            double baseRadiusX = 0.11;
            double baseRadiusY = 0.14;

            if (count == 3)
            {
                baseRadiusX = 0.16;
                baseRadiusY = 0.19;
            }
            else if (count == 4)
            {
                baseRadiusX = 0.14;
                baseRadiusY = 0.17;
            }

            std::vector<ContinentShape> continents;
            continents.reserve(
                static_cast<std::size_t>(count)
            );

            for (std::int32_t index = 0; index < count; ++index)
            {
                const std::uint64_t stream =
                    static_cast<std::uint64_t>(index) * 8ULL;

                const double jitterX =
                    (GenerationNoise::unit(
                        settings.seed,
                        stream + 1
                    ) * 2.0 - 1.0) * 0.012;

                const double jitterY =
                    (GenerationNoise::unit(
                        settings.seed,
                        stream + 2
                    ) * 2.0 - 1.0) * 0.012;

                const double radiusScaleX =
                    0.88
                    + GenerationNoise::unit(
                        settings.seed,
                        stream + 3
                    ) * 0.24;

                const double radiusScaleY =
                    0.88
                    + GenerationNoise::unit(
                        settings.seed,
                        stream + 4
                    ) * 0.24;

                const double rotation =
                    (GenerationNoise::unit(
                        settings.seed,
                        stream + 5
                    ) * 2.0 - 1.0) * 0.55;

                continents.push_back({
                    layout[static_cast<std::size_t>(index)][0]
                        + jitterX,
                    layout[static_cast<std::size_t>(index)][1]
                        + jitterY,
                    baseRadiusX * radiusScaleX,
                    baseRadiusY * radiusScaleY,
                    rotation,
                    GenerationNoise::mix(
                        settings.seed + stream + 6
                    )
                });
            }

            return continents;
        }
    }

    void LandmassGenerator::generate(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        const std::vector<ContinentShape> continents =
            createContinents(settings);

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const double normalizedX =
                    (static_cast<double>(x) + 0.5)
                    / static_cast<double>(grid.width());

                const double normalizedY =
                    (static_cast<double>(y) + 0.5)
                    / static_cast<double>(grid.height());

                double bestLandField = -4.0;

                for (const ContinentShape& continent : continents)
                {
                    const double offsetX =
                        normalizedX - continent.centerX;

                    const double offsetY =
                        normalizedY - continent.centerY;

                    const double cosine =
                        std::cos(continent.rotationRadians);

                    const double sine =
                        std::sin(continent.rotationRadians);

                    const double rotatedX =
                        offsetX * cosine
                        - offsetY * sine;

                    const double rotatedY =
                        offsetX * sine
                        + offsetY * cosine;

                    const double ellipticalDistance =
                        std::hypot(
                            rotatedX / continent.radiusX,
                            rotatedY / continent.radiusY
                        );

                    const double coastlineNoise =
                        GenerationNoise::fractal(
                            static_cast<double>(x) * 0.032,
                            static_cast<double>(y) * 0.032,
                            continent.noiseSeed,
                            4,
                            0.52,
                            2.05
                        );

                    const double detailNoise =
                        GenerationNoise::fractal(
                            static_cast<double>(x) * 0.095,
                            static_cast<double>(y) * 0.095,
                            continent.noiseSeed
                                ^ 0xA24B'AED4'963E'E407ULL,
                            2,
                            0.45,
                            2.3
                        );

                    const double landField =
                        1.0
                        - ellipticalDistance
                        + coastlineNoise * 0.18
                        + detailNoise * 0.045;

                    bestLandField =
                        std::max(bestLandField, landField);
                }

                const double elevation =
                    bestLandField >= 0.0
                        ? static_cast<double>(settings.seaLevel)
                            + bestLandField
                                * (1.0 - settings.seaLevel)
                                * 0.82
                        : static_cast<double>(settings.seaLevel)
                            + bestLandField * 0.24;

                WorldTile* tile =
                    grid.tile({x, y});

                tile->elevation = Elevation{
                    static_cast<float>(elevation)
                };

                tile->temperature = Temperature{};
                tile->rainfall = Rainfall{};

                const bool isLand =
                    tile->elevation.value()
                    > settings.seaLevel;

                tile->terrain = isLand
                    ? TerrainType::Land
                    : TerrainType::Water;

                tile->biome = isLand
                    ? BiomeType::Plain
                    : BiomeType::Ocean;
            }
        }
    }
}
