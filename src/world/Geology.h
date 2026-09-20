#pragma once

#include "world/WorldGrid.h"
#include "world/generation/GenerationNoise.h"

namespace Paladin
{
    // World deposits are the source for local veins and aggregate extraction.
    // Ore is material. It never credits a treasury or creates currency.
    inline void generateGeology(WorldGrid& grid, std::uint64_t seed)
    {
        for (int y = 0; y < grid.height(); ++y)
        {
            for (int x = 0; x < grid.width(); ++x)
            {
                auto* tile = grid.tile({x, y});
                tile->mineral = MineralDeposit::None;
                if (tile->terrain == TerrainType::Water ||
                    tile->biome == BiomeType::Polar)
                {
                    continue;
                }
                int rugged = 0;
                for (int dy = -2; dy <= 2; ++dy)
                {
                    for (int dx = -2; dx <= 2; ++dx)
                    {
                        const auto* near = grid.tile(
                            {(x + dx + grid.width()) % grid.width(), y + dy}
                        );
                        if (near && (near->terrain == TerrainType::Mountain ||
                                     near->relief != ReliefType::Lowland))
                        {
                            ++rugged;
                        }
                    }
                }
                const auto hash = GenerationNoise::mix(
                    seed ^ (std::uint64_t(x / 2) * 73856093ULL) ^
                    (std::uint64_t(y / 2) * 19349663ULL)
                );
                const auto roll = hash % 10000;
                const unsigned occurrence = 250 + rugged * 180;
                if (roll < occurrence)
                {
                    tile->mineral = (hash >> 20) % 100 < 2
                                        ? MineralDeposit::Gold
                                    : (hash >> 24) % 2 ? MineralDeposit::Iron
                                                       : MineralDeposit::Coal;
                }
            }
        }
    }
} // namespace Paladin
