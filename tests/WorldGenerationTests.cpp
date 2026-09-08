#include "TestFramework.h"

#include "rendering/WorldReliefPlacement.h"
#include "rendering/WorldSurface.h"
#include "world/BiomeType.h"
#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"
#include "world/World.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/generation/SettlementMapGenerator.h"
#include "world/generation/TerrainBiomeClassifier.h"
#include "world/generation/WorldGenerationSeed.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/generation/WorldRelief.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementNaturalFeatures.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace
{
    static_assert(!std::is_same_v<
                  Paladin::WorldTilePosition,
                  Paladin::SettlementTilePosition>);
    static_assert(!std::is_convertible_v<
                  Paladin::WorldTilePosition,
                  Paladin::SettlementTilePosition>);

    template<typename Grid> std::uint64_t worldHash(const Grid& grid)
    {
        std::uint64_t hash = 1'469'598'103'934'665'603ULL;

        constexpr std::uint64_t prime = 1'099'511'628'211ULL;

        const auto addValue = [&hash](std::uint64_t value)
        {
            hash ^= value;
            hash *= prime;
        };

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const Paladin::WorldTile* tile = grid.tile({x, y});

                addValue(static_cast<std::uint64_t>(tile->terrain));

                addValue(static_cast<std::uint64_t>(tile->biome));

                addValue(std::bit_cast<std::uint32_t>(tile->elevation.value()));

                addValue(
                    std::bit_cast<std::uint32_t>(tile->temperature.value())
                );

                addValue(std::bit_cast<std::uint32_t>(tile->rainfall.value()));
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

        PALADIN_CHECK(firstWorld.generationSeed() == settings.seed);

        PALADIN_CHECK(
            worldHash(firstWorld.grid()) == worldHash(secondWorld.grid())
        );

        settings.seed += 1;
        const Paladin::World differentWorld(settings);

        PALADIN_CHECK(
            worldHash(firstWorld.grid()) != worldHash(differentWorld.grid())
        );
    }

    void testFoothillsAndWaterTopology()
    {
        using namespace Paladin;
        WorldGrid grid(24, 24);
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 24; ++x)
            {
                grid.tile({x, y})->elevation = Elevation{.6F};
            }
        }
        // A small inland pinhole is filled, a substantial lake and an
        // edge-connected channel are retained, including diagonal access.
        grid.tile({5, 5})->elevation = Elevation{.2F};
        for (int y = 12; y < 16; ++y)
        {
            for (int x = 12; x < 16; ++x)
            {
                grid.tile({x, y})->elevation = Elevation{.2F};
            }
        }
        for (int x = 0; x < 5; ++x)
        {
            grid.tile({x, x})->elevation = Elevation{.2F};
        }
        generateWorldRelief(grid, .46F, 912);
        PALADIN_CHECK(grid.tile({5, 5})->elevation.value() <= .46F);
        // The first pinhole actually connects diagonally to the channel;
        // an isolated second hole must be treated differently.
        grid.tile({8, 6})->elevation = Elevation{.2F};
        generateWorldRelief(grid, .46F, 912);
        PALADIN_CHECK(grid.tile({8, 6})->elevation.value() > .46F);
        PALADIN_CHECK(grid.tile({13, 13})->elevation.value() <= .46F);
        PALADIN_CHECK(grid.tile({3, 3})->elevation.value() <= .46F);

        WorldGenerationSettings settings;
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 24; ++x)
            {
                grid.tile({x, y})->elevation = Elevation{.50F};
            }
        }
        grid.tile({12, 12})->elevation = Elevation{.90F};
        TerrainBiomeClassifier{}.classify(grid, settings);
        PALADIN_CHECK(grid.tile({12, 12})->terrain == TerrainType::Mountain);
        PALADIN_CHECK(grid.tile({11, 12})->biome == BiomeType::Hills);
        PALADIN_CHECK(grid.tile({11, 12})->terrain == TerrainType::Land);
        PALADIN_CHECK(reliefFootprintFits(grid, 11, 12, 1, 1, true));
        PALADIN_CHECK(!reliefFootprintFits(grid, 11, 12, 2, 1, true));
        PALADIN_CHECK(reliefFootprintFits(grid, 12, 12, 1, 1, false));
        PALADIN_CHECK(!reliefFootprintFits(grid, 11, 12, 1, 1, false));
        PALADIN_CHECK(grid.tile({2, 2})->biome != BiomeType::Hills);

        // Hills survive conversion to the local city map.
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 24; ++x)
            {
                grid.tile({x, y})->biome = BiomeType::Hills;
            }
        }
        SettlementMapGenerationSettings local;
        local.localTilesPerWorldTile = 4;
        auto city =
            SettlementMapGenerator{}.generate(grid, {5, 5}, 3, 3, 812, local);
        PALADIN_CHECK(city->grid().tile({6, 6})->biome == BiomeType::Hills);

        SettlementGrid plain(192, 192), hills(192, 192);
        for (int y = 0; y < 192; ++y)
        {
            for (int x = 0; x < 192; ++x)
            {
                auto t = *grid.tile({2, 2});
                t.terrain = TerrainType::Land;
                t.biome = BiomeType::Plain;
                *plain.tile({x, y}) = t;
                t.biome = BiomeType::Hills;
                *hills.tile({x, y}) = t;
            }
        }
        SettlementNaturalFeatures a(192, 192), b(192, 192);
        a.generate(plain, 901);
        b.generate(hills, 901);
        int plainRocks = 0, hillRocks = 0;
        for (int y = 0; y < 192; ++y)
        {
            for (int x = 0; x < 192; ++x)
            {
                plainRocks += a.at({x, y}).kind == NaturalFeatureKind::Rock;
                hillRocks += b.at({x, y}).kind == NaturalFeatureKind::Rock;
            }
        }
        PALADIN_CHECK(hillRocks > plainRocks * 1.4);
        const auto left = WorldSurface::sphere(0, .5),
                   right = WorldSurface::sphere(1, .5);
        PALADIN_CHECK(std::abs(left.x - right.x) < 1e-12);
        PALADIN_CHECK(std::abs(left.z - right.z) < 1e-12);
    }

    void testGeneratedWorldInvariants()
    {
        static_assert(
            !std::is_assignable_v<Paladin::Elevation&, Paladin::Temperature>
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
            const double latitude = (static_cast<double>(y) + 0.5) /
                                    static_cast<double>(grid.height());

            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const Paladin::WorldTile* tile = grid.tile({x, y});

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
                    equatorialTemperatureTotal += tile->temperature.value();

                    ++equatorialTileCount;
                }

                if (latitude <= 0.12 || latitude >= 0.88)
                {
                    polarTemperatureTotal += tile->temperature.value();

                    ++polarTileCount;
                }

                if (tile->terrain == Paladin::TerrainType::Mountain)
                {
                    ++mountainTileCount;
                }

                if (tile->terrain == Paladin::TerrainType::Water)
                {
                    ++waterTileCount;

                    PALADIN_CHECK(tile->biome == Paladin::BiomeType::Ocean);
                }
                else
                {
                    PALADIN_CHECK(tile->biome != Paladin::BiomeType::Ocean);
                }
            }
        }

        const double equatorialAverage =
            equatorialTemperatureTotal /
            static_cast<double>(equatorialTileCount);

        const double polarAverage =
            polarTemperatureTotal / static_cast<double>(polarTileCount);

        PALADIN_CHECK(equatorialAverage > polarAverage + 0.35);

        PALADIN_CHECK(mountainTileCount > 0);
        PALADIN_CHECK(waterTileCount > 0);
    }

    void testSettlementMapTranslatesSelectedRegion()
    {
        Paladin::WorldGrid sourceGrid(9, 9);

        for (std::int32_t y = 0; y < sourceGrid.height(); ++y)
        {
            for (std::int32_t x = 0; x < sourceGrid.width(); ++x)
            {
                Paladin::WorldTile* tile = sourceGrid.tile({x, y});

                tile->terrain = x < 4 ? Paladin::TerrainType::Water
                                      : Paladin::TerrainType::Land;

                tile->biome = x < 4 ? Paladin::BiomeType::Ocean
                                    : Paladin::BiomeType::Forest;

                tile->elevation = Paladin::Elevation(x < 4 ? 0.2F : 0.7F);

                tile->temperature = Paladin::Temperature(0.6F);
                tile->rainfall = Paladin::Rainfall(0.7F);
            }
        }

        Paladin::SettlementMapGenerationSettings settings;
        settings.localTilesPerWorldTile = 4;

        const Paladin::SettlementMapGenerator generator;
        const Paladin::WorldTilePosition selectedCenter{4, 4};

        const std::unique_ptr<Paladin::SettlementMap> first =
            generator.generate(
                sourceGrid,
                selectedCenter,
                9,
                9,
                0xABCD'1234ULL,
                settings
            );

        const std::unique_ptr<Paladin::SettlementMap> second =
            generator.generate(
                sourceGrid,
                selectedCenter,
                9,
                9,
                0xABCD'1234ULL,
                settings
            );

        PALADIN_CHECK(first != nullptr);
        PALADIN_CHECK(second != nullptr);
        PALADIN_CHECK(first->grid().width() == 36);
        PALADIN_CHECK(first->grid().height() == 36);
        PALADIN_CHECK(first->sourceRegionCenter() == selectedCenter);
        PALADIN_CHECK(first->sourceRegionWidth() == 9);
        PALADIN_CHECK(first->sourceRegionHeight() == 9);
        PALADIN_CHECK(first->localTilesPerWorldTile() == 4);
        PALADIN_CHECK(worldHash(first->grid()) == worldHash(second->grid()));

        PALADIN_CHECK(
            first->grid().tile({1, 18})->terrain == Paladin::TerrainType::Water
        );

        PALADIN_CHECK(
            first->grid().tile({34, 18})->terrain == Paladin::TerrainType::Land
        );

        const auto defaults = Paladin::defaultSettlementMapGenerationSettings();

        PALADIN_CHECK(defaults.localTilesPerWorldTile == 64);
        PALADIN_CHECK(9 * defaults.localTilesPerWorldTile == 576);
    }
} // namespace

void runWorldGenerationTests()
{
    const Paladin::WorldGenerationSettings firstRandomSettings =
        Paladin::withRandomWorldSeed();

    const Paladin::WorldGenerationSettings secondRandomSettings =
        Paladin::withRandomWorldSeed();

    PALADIN_CHECK(firstRandomSettings.seed != secondRandomSettings.seed);
    PALADIN_CHECK(
        firstRandomSettings.landmassTemplateId ==
        secondRandomSettings.landmassTemplateId
    );

    testDeterministicWorldGeneration();
    testFoothillsAndWaterTopology();
    testGeneratedWorldInvariants();
    testSettlementMapTranslatesSelectedRegion();
}
