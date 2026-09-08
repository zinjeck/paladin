#pragma once
#include "world/WorldGrid.h"
#include "world/generation/GenerationNoise.h"
#include <array>
#include <cmath>
#include <vector>
namespace Paladin
{
    // Generation only. No movement, combat, building, or production slope
    // rules.
    inline void generateWorldRelief(
        WorldGrid& grid,
        float sea,
        std::uint64_t seed
    )
    {
        const int w = grid.width(), h = grid.height();
        std::vector<bool> seen(std::size_t(w) * h);
        std::vector<int> region;
        constexpr int dx[] = {1, -1, 0, 0, 1, 1, -1, -1},
                      dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const int start = y * w + x;
                if (seen[start] || grid.tile({x, y})->elevation.value() > sea)
                {
                    continue;
                }
                region.clear();
                region.push_back(start);
                seen[start] = true;
                bool edge = false;
                for (std::size_t i = 0; i < region.size(); ++i)
                {
                    int xx = region[i] % w, yy = region[i] / w;
                    edge |= xx == 0 || yy == 0 || xx == w - 1 || yy == h - 1;
                    for (int n = 0; n < 8; ++n)
                    {
                        int nx = xx + dx[n], ny = yy + dy[n];
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        {
                            continue;
                        }
                        int at = ny * w + nx;
                        if (!seen[at] &&
                            grid.tile({nx, ny})->elevation.value() <= sea)
                        {
                            seen[at] = true;
                            region.push_back(at);
                        }
                    }
                }
                // Never close an ocean-connected bay/strait or a substantial
                // lake.
                if (!edge && region.size() <= 8)
                {
                    for (int at : region)
                    {
                        grid.tile({at % w, at / w})->elevation =
                            Elevation{sea + .025F};
                    }
                }
            }
        }
        struct Plate
        {
            double x, y;
        };
        std::array<Plate, 18> plates;
        for (unsigned i = 0; i < plates.size(); ++i)
        {
            auto a = GenerationNoise::mix(seed + i * 137),
                 b = GenerationNoise::mix(a);
            plates[i] = {
                double(a % 100000) / 100000,
                double(b % 100000) / 100000
            };
        }
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                auto* t = grid.tile({x, y});
                if (t->elevation.value() <= sea)
                {
                    continue;
                }
                double u = double(x) / w, v = double(y) / h;
                u += .035 * GenerationNoise::simplexFractal(
                                u * 5,
                                v * 5,
                                seed + 51,
                                2,
                                .5,
                                2
                            );
                v += .035 * GenerationNoise::simplexFractal(
                                u * 5,
                                v * 5,
                                seed + 89,
                                2,
                                .5,
                                2
                            );
                double first = 10, second = 10;
                unsigned a = 0, b = 0;
                for (unsigned i = 0; i < plates.size(); ++i)
                {
                    double d = std::hypot(u - plates[i].x, v - plates[i].y);
                    if (d < first)
                    {
                        second = first;
                        b = a;
                        first = d;
                        a = i;
                    }
                    else if (d < second)
                    {
                        second = d;
                        b = i;
                    }
                }
                const auto pair = std::min(a, b) * 31 + std::max(a, b);
                const bool convergent =
                    GenerationNoise::mix(seed + pair) % 3 != 0;
                double ridge =
                    convergent ? std::exp(-std::pow((second - first) / .018, 2))
                               : 0;
                const double base = (t->elevation.value() - sea) / (1 - sea);
                const double inland = std::clamp(base * 9., 0., 1.);
                const double relief =
                    std::clamp(base * .30 + ridge * .66 * inland, 0., 1.);
                t->elevation = Elevation{float(sea + (1 - sea) * relief)};
            }
        }
    }
} // namespace Paladin
