#include "TestFramework.h"

#include "world/BiomeType.h"
#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"
#include "world/World.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/generation/WorldGenerationSettings.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <type_traits>
#include <vector>

namespace
{
    struct LandmassAnalysis
    {
        std::size_t majorLandmassCount = 0;
        std::size_t totalLandTileCount = 0;
        std::size_t largestLandmassTileCount = 0;
    };

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

    std::uint64_t worldHash(
        const Paladin::WorldGrid& grid
    )
    {
        std::uint64_t hash =
            1'469'598'103'934'665'603ULL;

        constexpr std::uint64_t prime =
            1'099'511'628'211ULL;

        const auto addValue = [&hash](
            std::uint64_t value
        )
        {
            hash ^= value;
            hash *= prime;
        };

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const Paladin::WorldTile* tile =
                    grid.tile({x, y});

                addValue(
                    static_cast<std::uint64_t>(tile->terrain)
                );

                addValue(
                    static_cast<std::uint64_t>(tile->biome)
                );

                addValue(std::bit_cast<std::uint32_t>(
                    tile->elevation.value()
                ));

                addValue(std::bit_cast<std::uint32_t>(
                    tile->temperature.value()
                ));

                addValue(std::bit_cast<std::uint32_t>(
                    tile->rainfall.value()
                ));
            }
        }

        return hash;
    }

    LandmassAnalysis analyzeLandmasses(
        const Paladin::WorldGrid& grid
    )
    {
        LandmassAnalysis analysis;

        std::vector<bool> visited(
            grid.tileCount(),
            false
        );

        std::vector<std::size_t> componentSizes;

        constexpr std::int32_t neighborOffsets[4][2] = {
            {-1, 0},
            {1, 0},
            {0, -1},
            {0, 1}
        };

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const std::size_t startIndex =
                    tileIndex(x, y, grid.width());

                const Paladin::WorldTile* startTile =
                    grid.tile({x, y});

                if (
                    visited[startIndex] ||
                    startTile->terrain == Paladin::TerrainType::Water
                )
                {
                    continue;
                }

                std::deque<std::size_t> openTiles{
                    startIndex
                };

                visited[startIndex] = true;
                std::size_t componentSize = 0;

                while (!openTiles.empty())
                {
                    const std::size_t currentIndex =
                        openTiles.front();

                    openTiles.pop_front();
                    ++componentSize;

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

                        const Paladin::WorldTile* neighborTile =
                            grid.tile({neighborX, neighborY});

                        if (
                            visited[neighborIndex] ||
                            neighborTile->terrain
                                == Paladin::TerrainType::Water
                        )
                        {
                            continue;
                        }

                        visited[neighborIndex] = true;
                        openTiles.push_back(neighborIndex);
                    }
                }

                componentSizes.push_back(componentSize);
                analysis.totalLandTileCount += componentSize;

                if (
                    componentSize
                    > analysis.largestLandmassTileCount
                )
                {
                    analysis.largestLandmassTileCount =
                        componentSize;
                }
            }
        }

        const std::size_t minimumMajorSize =
            grid.tileCount() / 200;

        for (const std::size_t componentSize : componentSizes)
        {
            if (componentSize >= minimumMajorSize)
            {
                ++analysis.majorLandmassCount;
            }
        }

        return analysis;
    }

    void testDeterministicWorldGeneration()
    {
        Paladin::WorldGenerationSettings settings;
        settings.width = 180;
        settings.height = 132;
        settings.seed = 0x1234'5678ULL;

        const Paladin::World firstWorld(settings);
        const Paladin::World secondWorld(settings);

        PALADIN_CHECK(
            firstWorld.generationSeed() == settings.seed
        );

        PALADIN_CHECK(
            worldHash(firstWorld.grid())
            == worldHash(secondWorld.grid())
        );

        settings.seed += 1;
        const Paladin::World differentWorld(settings);

        PALADIN_CHECK(
            worldHash(firstWorld.grid())
            != worldHash(differentWorld.grid())
        );
    }

    void testGeneratedWorldInvariants()
    {
        static_assert(
            !std::is_assignable_v<
                Paladin::Elevation&,
                Paladin::Temperature
            >
        );

        Paladin::WorldGenerationSettings settings;
        settings.width = 180;
        settings.height = 132;
        settings.seed = 0xCAFE'BEEFULL;

        const Paladin::World world(settings);
        const Paladin::WorldGrid& grid = world.grid();

        double equatorialTemperatureTotal = 0.0;
        std::size_t equatorialTileCount = 0;
        double polarTemperatureTotal = 0.0;
        std::size_t polarTileCount = 0;
        std::size_t mountainTileCount = 0;
        std::size_t waterTileCount = 0;

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            const double latitude =
                (static_cast<double>(y) + 0.5)
                / static_cast<double>(grid.height());

            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const Paladin::WorldTile* tile =
                    grid.tile({x, y});

                PALADIN_CHECK(
                    tile->elevation.value() >= 0.0F &&
                    tile->elevation.value() <= 1.0F
                );

                PALADIN_CHECK(
                    tile->temperature.value() >= 0.0F &&
                    tile->temperature.value() <= 1.0F
                );

                PALADIN_CHECK(
                    tile->rainfall.value() >= 0.0F &&
                    tile->rainfall.value() <= 1.0F
                );

                if (latitude >= 0.42 && latitude <= 0.58)
                {
                    equatorialTemperatureTotal +=
                        tile->temperature.value();

                    ++equatorialTileCount;
                }

                if (latitude <= 0.12 || latitude >= 0.88)
                {
                    polarTemperatureTotal +=
                        tile->temperature.value();

                    ++polarTileCount;
                }

                if (
                    tile->terrain
                    == Paladin::TerrainType::Mountain
                )
                {
                    ++mountainTileCount;
                }

                if (
                    tile->terrain
                    == Paladin::TerrainType::Water
                )
                {
                    ++waterTileCount;

                    PALADIN_CHECK(
                        tile->biome == Paladin::BiomeType::Ocean
                    );
                }
                else
                {
                    PALADIN_CHECK(
                        tile->biome != Paladin::BiomeType::Ocean
                    );
                }
            }
        }

        const double equatorialAverage =
            equatorialTemperatureTotal
            / static_cast<double>(equatorialTileCount);

        const double polarAverage =
            polarTemperatureTotal
            / static_cast<double>(polarTileCount);

        PALADIN_CHECK(
            equatorialAverage > polarAverage + 0.35
        );

        PALADIN_CHECK(mountainTileCount > 0);
        PALADIN_CHECK(waterTileCount > 0);

        const LandmassAnalysis landmasses =
            analyzeLandmasses(grid);

        PALADIN_CHECK(
            landmasses.majorLandmassCount >= 3 &&
            landmasses.majorLandmassCount <= 5
        );

        PALADIN_CHECK(
            landmasses.totalLandTileCount > 0
        );

        const double largestLandmassFraction =
            static_cast<double>(
                landmasses.largestLandmassTileCount
            )
            / static_cast<double>(
                landmasses.totalLandTileCount
            );

        PALADIN_CHECK(largestLandmassFraction < 0.55);
    }
}

void runWorldGenerationTests()
{
    testDeterministicWorldGeneration();
    testGeneratedWorldInvariants();
}
