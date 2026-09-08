#pragma once
#include "rendering/TerrainMaterialField.h"
#include "world/Settlement.h"
#include "world/World.h"
#include <unordered_set>

namespace Paladin
{
    inline bool forestBiome(BiomeType biome)
    {
        return biome == BiomeType::Forest || biome == BiomeType::Jungle ||
               biome == BiomeType::Taiga;
    }
    inline bool canopyCleared(const World& world, int x, int y)
    {
        for (const auto& city : world.settlements())
        {
            const auto p = city.position();
            if (std::abs(p.x - x) <= 1 && std::abs(p.y - y) <= 1)
            {
                return true;
            }
        }
        return false;
    }
    inline void worldFoliage(
        Renderer& renderer,
        const World& world,
        const SceneProjection& p,
        const SceneSpriteLibrary& art
    )
    {
        if (p.tilePixels < 6)
        {
            return;
        }
        const int x0 = std::max(
            0,
            int(std::floor(p.cameraX - p.screenWidth * .5 / p.tilePixels)) - 2
        );
        const int y0 = std::max(
            0,
            int(std::floor(p.cameraY - p.screenHeight * .5 / p.tilePixels)) - 2
        );
        const int x1 = std::min(
            world.grid().width(),
            int(std::ceil(p.cameraX + p.screenWidth * .5 / p.tilePixels)) + 2
        );
        const int y1 = std::min(
            world.grid().height(),
            int(std::ceil(p.cameraY + p.screenHeight * .5 / p.tilePixels)) + 2
        );
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const auto& tile = *world.grid().tile({x, y});
                if (tile.terrain != TerrainType::Land ||
                    !forestBiome(tile.biome) || canopyCleared(world, x, y))
                {
                    continue;
                }
                const auto hash = landscapeHash(x, y, 171);
                const double density = landscapeField(x * .17, y * .17, 117);
                if ((hash & 255) / 255. > .25 + density * .65)
                {
                    continue;
                }
                const char* name = tile.biome == BiomeType::Taiga
                                       ? "world.canopy.2"
                                       : "world.canopy.1";
                const auto* sprite = art.find(name);
                if (!sprite)
                {
                    continue;
                }
                const double size = .7 + ((hash >> 12) % 8) * .08;
                const double xx = x + .1 + ((hash >> 8) % 7) * .08,
                             yy = y + .1 + ((hash >> 20) % 7) * .08;
                const auto b = p.bounds({xx, yy, 0, size, size * .75, .5, .5});
                const auto f = art.frame(*sprite, false);
                renderer.drawTexture(
                    *sprite->texture,
                    f.x,
                    f.y,
                    f.width,
                    f.height,
                    b.x,
                    b.y,
                    b.width,
                    b.height
                );
            }
        }
    }
} // namespace Paladin
