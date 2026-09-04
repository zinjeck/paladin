#include "world/generation/ClimateGenerator.h"

#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/generation/GenerationNoise.h"
#include "world/generation/WorldGenerationSettings.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <vector>

namespace Paladin
{
    namespace
    {
        std::size_t tileIndex(
            std::int32_t x,
            std::int32_t y,
            std::int32_t width
        ) noexcept
        {
            return
                static_cast<std::size_t>(y)
                * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x);
        }

        std::vector<std::int32_t> distanceToWater(
            const WorldGrid& grid
        )
        {
            constexpr std::int32_t maximumDistance = 64;

            std::vector<std::int32_t> distances(
                grid.tileCount(),
                maximumDistance
            );

            std::deque<std::size_t> openTiles;

            for (std::int32_t y = 0; y < grid.height(); ++y)
            {
                for (std::int32_t x = 0; x < grid.width(); ++x)
                {
                    const WorldTile* tile =
                        grid.tile({x, y});

                    if (tile->terrain != TerrainType::Water)
                    {
                        continue;
                    }

                    const std::size_t index =
                        tileIndex(x, y, grid.width());

                    distances[index] = 0;
                    openTiles.push_back(index);
                }
            }

            constexpr std::int32_t neighborOffsets[4][2] = {
                {-1, 0},
                {1, 0},
                {0, -1},
                {0, 1}
            };

            while (!openTiles.empty())
            {
                const std::size_t currentIndex =
                    openTiles.front();

                openTiles.pop_front();

                const auto currentX =
                    static_cast<std::int32_t>(
                        currentIndex
                        % static_cast<std::size_t>(grid.width())
                    );

                const auto currentY =
                    static_cast<std::int32_t>(
                        currentIndex
                        / static_cast<std::size_t>(grid.width())
                    );

                const std::int32_t nextDistance =
                    distances[currentIndex] + 1;

                if (nextDistance > maximumDistance)
                {
                    continue;
                }

                for (const auto& offset : neighborOffsets)
                {
                    const std::int32_t neighborX =
                        currentX + offset[0];

                    const std::int32_t neighborY =
                        currentY + offset[1];

                    if (!grid.isValidPosition({
                        neighborX,
                        neighborY
                    }))
                    {
                        continue;
                    }

                    const std::size_t neighborIndex =
                        tileIndex(
                            neighborX,
                            neighborY,
                            grid.width()
                        );

                    if (distances[neighborIndex] <= nextDistance)
                    {
                        continue;
                    }

                    distances[neighborIndex] = nextDistance;
                    openTiles.push_back(neighborIndex);
                }
            }

            return distances;
        }
    }

    void ClimateGenerator::generate(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        const std::vector<std::int32_t> waterDistances =
            distanceToWater(grid);

        constexpr double pi = 3.14159265358979323846;

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            const double normalizedLatitude =
                (static_cast<double>(y) + 0.5)
                / static_cast<double>(grid.height());

            const double latitudeHeat =
                std::sin(normalizedLatitude * pi);

            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                WorldTile* tile =
                    grid.tile({x, y});

                const double landElevation =
                    std::max(
                        0.0,
                        (
                            static_cast<double>(
                                tile->elevation.value()
                            )
                            - settings.seaLevel
                        )
                        / (1.0 - settings.seaLevel)
                    );

                const double temperatureNoise =
                    GenerationNoise::fractal(
                        static_cast<double>(x) * 0.018,
                        static_cast<double>(y) * 0.018,
                        settings.seed
                            ^ 0x7C15'1A2B'3C4D'5E6FULL,
                        3,
                        0.5,
                        2.0
                    );

                const double temperature =
                    0.06
                    + latitudeHeat * 0.92
                    - landElevation * 0.30
                    + temperatureNoise * 0.07;

                tile->temperature = Temperature{
                    static_cast<float>(temperature)
                };

                const double rainfallNoise =
                    GenerationNoise::fractal(
                        static_cast<double>(x) * 0.021,
                        static_cast<double>(y) * 0.021,
                        settings.seed
                            ^ 0x4211'3B8D'6F20'9AC5ULL,
                        4,
                        0.52,
                        2.0
                    ) * 0.5 + 0.5;

                const std::size_t index =
                    tileIndex(x, y, grid.width());

                const double coastalMoisture =
                    std::exp(
                        -static_cast<double>(waterDistances[index])
                        / 28.0
                    );

                const double rainfall =
                    0.10
                    + rainfallNoise * 0.48
                    + coastalMoisture * 0.27
                    + tile->temperature.value() * 0.10
                    - landElevation * 0.12;

                tile->rainfall = Rainfall{
                    static_cast<float>(rainfall)
                };
            }
        }
    }
}
