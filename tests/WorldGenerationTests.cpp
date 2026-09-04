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
#include <type_traits>

namespace
{
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
    }
}

void runWorldGenerationTests()
{
    testDeterministicWorldGeneration();
    testGeneratedWorldInvariants();
}
