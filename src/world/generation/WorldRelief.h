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
        // Godot Paladin: broad four-octave mountain noise plus an elevation
        // bonus, then supported peak centers surrounded by foothills.
        std::vector<double> scores(std::size_t(w) * h);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const auto elevation = grid.tile({x, y})->elevation.value();
                scores[y * w + x] = std::abs(
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
        }
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                auto& tile = *grid.tile({x, y});
                if (tile.elevation.value() <= sea)
                {
                    continue;
                }
                const double score = scores[y * w + x];
                int support = 0;
                for (int j = -2; j <= 2; ++j)
                {
                    for (int i = -2; i <= 2; ++i)
                    {
                        if (!i && !j)
                        {
                            continue;
                        }
                        const int nx = x + i, ny = y + j;
                        if (nx >= 0 && ny >= 0 && nx < w && ny < h &&
                            grid.tile({nx, ny})->elevation.value() > sea &&
                            scores[ny * w + nx] > .62)
                        {
                            ++support;
                        }
                    }
                }
                const bool peak = score >= .76 && support >= 12;
                // Preserve smooth height within each band, not quantized
                // terraces.
                double height =
                    score > .62
                        ? .25 + std::clamp((score - .62) / .14, 0., 1.) * .26
                        : .02 + std::clamp(score / .62, 0., 1.) * .22;
                if (peak)
                {
                    height =
                        .52 + std::clamp((score - .76) / .40, 0., 1.) * .46;
                }
                tile.elevation = Elevation{float(sea + (1 - sea) * height)};
            }
        }
    }
} // namespace Paladin
