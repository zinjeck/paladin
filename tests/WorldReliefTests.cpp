#include "TestFramework.h"
#include "world/generation/WorldRelief.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace
{
    using namespace Paladin;

    // Contract copied from the Godot reference: coherent four-octave score,
    // .62 foothill threshold and .76 peaks with twelve nearby hill supports.
    double godotScore(int x, int y, float elevation, std::uint64_t seed)
    {
        return std::abs(
                   GenerationNoise::simplexFractal(
                       x * .018,
                       y * .018,
                       seed + 73517,
                       4,
                       .58,
                       2.25
                   )
               ) +
               elevation * .55;
    }

    WorldGrid uniformGrid(int width, int height, float elevation)
    {
        WorldGrid grid(width, height);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                grid.tile({x, y})->elevation = Elevation{elevation};
            }
        }
        return grid;
    }

    void supportedRangesAndDeterminism()
    {
        constexpr int width = 180;
        constexpr int height = 132;
        constexpr float sea = .4F;
        constexpr float initial = .65F;
        for (std::uint64_t seed : {0ULL, 123456ULL, 987654321ULL})
        {
            auto grid = uniformGrid(width, height, initial);
            auto repeated = uniformGrid(width, height, initial);
            generateWorldRelief(grid, sea, seed);
            generateWorldRelief(repeated, sea, seed);
            int peaks = 0, foothills = 0;
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const float actual = grid.tile({x, y})->elevation.value();
                    PALADIN_CHECK(
                        std::isfinite(actual) && actual > sea && actual <= 1
                    );
                    const double score = godotScore(x, y, initial, seed);
                    int support = 0;
                    for (int j = -2; j <= 2; ++j)
                    {
                        for (int i = -2; i <= 2; ++i)
                        {
                            if ((!i && !j) || x + i < 0 || y + j < 0 ||
                                x + i >= width || y + j >= height)
                            {
                                continue;
                            }
                            support +=
                                godotScore(x + i, y + j, initial, seed) > .62;
                        }
                    }
                    const double height = (actual - sea) / (1 - sea);
                    if (score >= .76 && support >= 12)
                    {
                        PALADIN_CHECK(height >= .51999);
                        ++peaks;
                    }
                    else if (score > .62)
                    {
                        PALADIN_CHECK(height >= .24999 && height < .52);
                        ++foothills;
                    }
                    else
                    {
                        PALADIN_CHECK(height < .25);
                    }
                    PALADIN_CHECK(
                        actual == repeated.tile({x, y})->elevation.value()
                    );
                }
            }
            PALADIN_CHECK(peaks > 15 && foothills > 100);
        }
    }

    void waterPreservation()
    {
        constexpr float sea = .4F;
        constexpr float water = .2F;
        auto grid = uniformGrid(33, 23, .65F);
        // A bay plus a diagonal connection must remain connected to the edge.
        for (int x = 0; x <= 7; ++x)
        {
            grid.tile({x, 4})->elevation = Elevation{water};
        }
        grid.tile({8, 5})->elevation = Elevation{water};
        // A tiny enclosed pinhole is filled; a nine-tile lake is not.
        grid.tile({15, 10})->elevation = Elevation{water};
        grid.tile({16, 10})->elevation = Elevation{water};
        for (int y = 10; y < 13; ++y)
        {
            for (int x = 22; x < 25; ++x)
            {
                grid.tile({x, y})->elevation = Elevation{water};
            }
        }
        generateWorldRelief(grid, sea, 7123);
        for (int x = 0; x <= 7; ++x)
        {
            PALADIN_CHECK(grid.tile({x, 4})->elevation.value() == water);
        }
        PALADIN_CHECK(grid.tile({8, 5})->elevation.value() == water);
        PALADIN_CHECK(grid.tile({15, 10})->elevation.value() > sea);
        PALADIN_CHECK(grid.tile({16, 10})->elevation.value() > sea);
        for (int y = 10; y < 13; ++y)
        {
            for (int x = 22; x < 25; ++x)
            {
                PALADIN_CHECK(grid.tile({x, y})->elevation.value() == water);
            }
        }
        auto ocean = uniformGrid(1, 1, water);
        generateWorldRelief(ocean, sea, 0);
        PALADIN_CHECK(ocean.tile({0, 0})->elevation.value() == water);
    }
} // namespace

void runWorldReliefTests()
{
    supportedRangesAndDeterminism();
    waterPreservation();
}
