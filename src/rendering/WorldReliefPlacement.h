#pragma once
#include "world/WorldGrid.h"
namespace Paladin
{
    inline bool reliefFootprintFits(
        const WorldGrid& grid,
        int x,
        int y,
        int width,
        int height,
        bool hill
    )
    {
        for (int yy = y; yy < y + height; ++yy)
        {
            for (int xx = x; xx < x + width; ++xx)
            {
                const auto* t = grid.tile({xx, yy});
                if (!t || (hill ? t->terrain != TerrainType::Land ||
                                      (t->biome != BiomeType::Hills &&
                                       t->relief != ReliefType::Hills)
                                : t->terrain != TerrainType::Mountain))
                {
                    return false;
                }
            }
        }
        return true;
    }
} // namespace Paladin
