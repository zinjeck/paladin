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

    // A scalar reference on uniformly dry land, independent of the production
    // nearest-plate loop. Both perturbations sample the ORIGINAL coordinate.
    float referenceElevation(
        int x, int y, int width, int height,
        float inputElevation, float sea, std::uint64_t seed
    )
    {
        const double sourceU = double(x) / width;
        const double sourceV = double(y) / height;
        const std::array offsets{
            GenerationNoise::simplexFractal(sourceU * 5, sourceV * 5, seed + 51, 2, .5, 2),
            GenerationNoise::simplexFractal(sourceU * 5, sourceV * 5, seed + 89, 2, .5, 2)
        };
        const double u = sourceU + .035 * offsets[0];
        const double v = sourceV + .035 * offsets[1];
        std::array<std::pair<double, unsigned>, 18> distances;
        for (unsigned i = 0; i < distances.size(); ++i)
        {
            const auto a = GenerationNoise::mix(seed + i * 137);
            const auto b = GenerationNoise::mix(a);
            distances[i] = {
                std::hypot(u - double(a % 100000) / 100000,
                           v - double(b % 100000) / 100000),
                i
            };
        }
        std::sort(distances.begin(), distances.end());
        const auto pair = std::min(distances[0].second, distances[1].second) * 31 +
                          std::max(distances[0].second, distances[1].second);
        const double gap = distances[1].first - distances[0].first;
        const double ridge = GenerationNoise::mix(seed + pair) % 3 != 0
                                 ? std::exp(-std::pow(gap / .018, 2))
                                 : 0;
        const double base = (inputElevation - sea) / (1 - sea);
        const double inland = std::clamp(base * 9., 0., 1.);
        const double relief = std::clamp(base * .30 + ridge * .66 * inland, 0., 1.);
        return float(sea + (1 - sea) * relief);
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

    void independentWarpAndDeterminism()
    {
        constexpr int width = 47;
        constexpr int height = 31;
        constexpr float sea = .4F;
        constexpr float initial = .65F;
        for (std::uint64_t seed : {0ULL, 123456ULL, 987654321ULL})
        {
            auto grid = uniformGrid(width, height, initial);
            auto repeated = uniformGrid(width, height, initial);
            generateWorldRelief(grid, sea, seed);
            generateWorldRelief(repeated, sea, seed);
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const float actual = grid.tile({x, y})->elevation.value();
                    const float expected = referenceElevation(x, y, width, height, initial, sea, seed);
                    PALADIN_CHECK(std::isfinite(actual));
                    PALADIN_CHECK(actual > sea && actual <= 1);
                    if (std::abs(actual - expected) > 1e-6F)
                    {
                        throw std::runtime_error(
                            "Relief must sample both warp offsets from the unwarped coordinate."
                        );
                    }
                    PALADIN_CHECK(actual == repeated.tile({x, y})->elevation.value());
                }
            }
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
    independentWarpAndDeterminism();
    waterPreservation();
}
