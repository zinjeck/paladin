#pragma once
#include "rendering/GlobeLighting.h"
#include "rendering/GlobeView.h"
#include "rendering/SceneDetail.h"
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
    // A single world-feature definition, projected into either presentation.
    inline void worldFoliageProjected(
        Renderer& r,
        const World& world,
        const Camera2D& camera,
        double pixels,
        bool globe,
        const SceneSpriteLibrary& art
    )
    {
        if (pixels < 24)
        {
            return;
        }
        const auto& g = world.grid();
        const auto view =
            GlobeView::from(camera, g, r.outputWidth(), r.outputHeight());
        const double latitude =
            PlanetAstronomy::latitude(camera.tileY() / g.height());
        const double span =
            globe ? std::asin(
                        std::min(
                            1.,
                            std::hypot(r.outputWidth(), r.outputHeight()) * .5 /
                                view.radius
                        )
                    )
                  : 0.;
        const int ry = globe ? int(span * g.height() / PlanetAstronomy::Pi) + 3
                             : int(r.outputHeight() / (2 * pixels)) + 3;
        const double minCos = std::cos(
            std::min(PlanetAstronomy::Pi * .5, std::abs(latitude) + span)
        );
        const int rx =
            globe
                ? (minCos < .01
                       ? g.width() / 2 + 1
                       : std::min(
                             g.width() / 2 + 1,
                             int(span * g.width() / (6.283185307 * minCos)) + 3
                         ))
                : int(r.outputWidth() / (2 * pixels)) + 3;
        const int start = int(camera.tileX()) - rx,
                  count = std::min(g.width(), 2 * rx + 1);
        for (int y = std::max(0, int(camera.tileY()) - ry);
             y < std::min(g.height(), int(camera.tileY()) + ry + 1);
             ++y)
        {
            for (int i = 0; i < count; ++i)
            {
                const int x = ((start + i) % g.width() + g.width()) % g.width();
                const auto& t = *g.tile({x, y});
                if (t.terrain != TerrainType::Land || !forestBiome(t.biome) ||
                    canopyCleared(world, x, y))
                {
                    continue;
                }
                const auto hash = landscapeHash(x, y, 171);
                if ((hash & 255) / 255. >
                    .25 + landscapeField(x * .17, y * .17, 117) * .65)
                {
                    continue;
                }
                const double u = (x + .3 + (hash % 8) * .07) / g.width(),
                             v = (y + .3) / g.height();
                WorldSurface::Point3 p;
                if (globe)
                {
                    p = view.project(u, v);
                }
                else
                {
                    double dx = u * g.width() - camera.tileX();

                    p = {
                        r.outputWidth() * .5 + dx * pixels,
                        r.outputHeight() * .5 +
                            (v * g.height() - camera.tileY()) * pixels,
                        1
                    };
                }
                if (p.z <= 0 || p.x < -pixels || p.y < -pixels ||
                    p.x > r.outputWidth() + pixels ||
                    p.y > r.outputHeight() + pixels)
                {
                    continue;
                }
                const auto* sprite = art.find(
                    t.biome == BiomeType::Taiga ? "world.canopy.2"
                                                : "world.canopy.1"
                );
                if (!sprite)
                {
                    continue;
                }
                const auto f = art.frame(*sprite, false);
                auto tint =
                    globeLight(u, v, world.time().secondsIntoDay(), p.z);
                tint.alpha =
                    std::uint8_t(tint.alpha * detailBlend(pixels, 24, 36));
                const float x0 = float(p.x - pixels * .5),
                            y0 = float(p.y - pixels * .4),
                            x1 = float(x0 + pixels),
                            y1 = float(y0 + pixels * .75);
                const float u0 = float(f.x) / sprite->texture->width(),
                            v0 = float(f.y) / sprite->texture->height(),
                            u1 =
                                float(f.x + f.width) / sprite->texture->width(),
                            v1 = float(f.y + f.height) /
                                 sprite->texture->height();
                const std::array<MeshVertex, 4> quad{
                    {{x0, y0, u0, v0, tint},
                     {x1, y0, u1, v0, tint},
                     {x1, y1, u1, v1, tint},
                     {x0, y1, u0, v1, tint}}
                };
                const std::array<int, 6> ids{0, 1, 2, 0, 2, 3};
                r.drawMesh(*sprite->texture, quad, ids);
            }
        }
    }
} // namespace Paladin
