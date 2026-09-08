#pragma once
#include "TestFramework.h"
#include "rendering/BuildingView.h"
#include "rendering/SettlementGroundCache.h"
#include "rendering/WorldGridRenderer.h"
#include "rendering/WorldSurface.h"
#include "world/WorldGrid.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <filesystem>
#include <unordered_set>

namespace Paladin
{
    inline void renderingRevisionChecks(
        Renderer& renderer,
        SDL_Renderer* native
    )
    {
        SceneSpriteLibrary art;
        art.load(
            renderer,
            (std::filesystem::path(SDL_GetBasePath()) / "assets/sprites")
                .string()
        );
        const auto& home = art.objectStyle("house");
        PALADIN_CHECK(home.wall == art.objectStyle("bakery").wall);
        PALADIN_CHECK(home.wall == art.objectStyle("city_keep").wall);
        PALADIN_CHECK(home.roof == art.objectStyle("city_keep").roof);
        const auto* roof = art.find(home.roof + ".full");
        PALADIN_CHECK(roof);
        for (int i = 1; i <= 4; ++i)
        {
            const auto* variant =
                art.find(home.roof + ".full." + std::to_string(i));
            PALADIN_CHECK(
                variant && variant->width == roof->width &&
                variant->height == roof->height &&
                variant->elevation == roof->elevation
            );
        }
        std::unordered_set<const Texture*> biomes;
        for (const auto* name :
             {"plain",
              "forest",
              "jungle",
              "desert",
              "tundra",
              "taiga",
              "water",
              "shallow",
              "beach"})
        {
            const auto* terrain =
                art.find(std::string("world.terrain.") + name);
            PALADIN_CHECK(terrain && terrain->texture);
            PALADIN_CHECK(
                terrain->texture != art.find("terrain.plain")->texture
            );
            biomes.insert(terrain->texture.get());
        }
        PALADIN_CHECK(biomes.size() == 9);

        // Fitted keep roofs must have no background cracks at animated strip
        // joins. Use the real opaque roof center and the final pixel scene.
        for (double time : {0., .3, 1.})
        {
            art.setTime(time);
            renderer.beginFrame();
            {
                WorldPixelScene pixels(renderer, 64);
                SceneProjection view{
                    0,
                    0,
                    64,
                    renderer.outputWidth(),
                    renderer.outputHeight()
                };
                SceneDrawQueue roofQueue;
                PALADIN_CHECK(art.placed(
                    roofQueue,
                    view,
                    "roof.thatch.full.side.1",
                    -1.72,
                    -3.54 + .85,
                    0,
                    1,
                    10,
                    3.44,
                    7.08
                ));
                roofQueue.render(renderer);
            }
            auto* image = SDL_RenderReadPixels(native, nullptr);
            PALADIN_CHECK(image);
            const int top = renderer.outputHeight() / 2 - int(3.54 * 64);
            for (int y = top + 24; y < top + int(7.08 * 64 * .94); ++y)
            {
                Uint8 r, g, b, a;
                PALADIN_CHECK(SDL_ReadSurfacePixel(
                    image,
                    renderer.outputWidth() / 2,
                    y,
                    &r,
                    &g,
                    &b,
                    &a
                ));
                PALADIN_CHECK(!(r == 18 && g == 20 && b == 24));
            }
            SDL_DestroySurface(image);
        }
        art.setTime(0);

        // Forward/inverse planet coordinates, rotation, longitude seam and sky
        // rejection.
        for (double u : {.01, .25, .5, .99})
        {
            for (double v : {.1, .5, .9})
            {
                const auto original = WorldSurface::sphere(u, v);
                const auto roundTrip = WorldSurface::coordinates(original);
                PALADIN_CHECK(
                    std::abs(roundTrip.u - u) < 1e-10 &&
                    std::abs(roundTrip.v - v) < 1e-10
                );
                const auto rotated = WorldSurface::orient(original, .8, -.3);
                if (rotated.z > 0)
                {
                    const auto pick =
                        WorldSurface::pick(rotated.x, rotated.y, .8, -.3);
                    PALADIN_CHECK(
                        pick && std::abs(pick->u - u) < 1e-10 &&
                        std::abs(pick->v - v) < 1e-10
                    );
                }
            }
        }
        PALADIN_CHECK(!WorldSurface::pick(1.1, 0, 0, 0));
        PALADIN_CHECK(
            std::abs(
                WorldSurface::sphere(0, .5).x - WorldSurface::sphere(1, .5).x
            ) < 1e-10
        );
        // A straight logical boundary acquires stable small coves, not camera
        // noise.
        double smallest = 1, largest = -1;
        for (int y = 0; y < 128; ++y)
        {
            const auto p = coastSample(16, y / 8., false);
            smallest = std::min(smallest, p.x - 16);
            largest = std::max(largest, p.x - 16);
            PALADIN_CHECK(
                std::abs(p.x - 16) < .25 && std::abs(p.y - y / 8.) < .25
            );
        }
        PALADIN_CHECK(largest - smallest > .15);

        SettlementGrid grid(32, 32);
        for (int y = 0; y < 32; ++y)
        {
            for (int x = 0; x < 32; ++x)
            {
                grid.tile({x, y})->terrain = TerrainType::Land;
                grid.tile({x, y})->biome = BiomeType::Plain;
            }
        }
        SettlementMap map(std::move(grid), {100, 100}, 1, 1, 32, 8721);
        auto road =
            *SettlementObjectCatalog::definition(SettlementObjectTypes::Road);
        road.bypassesConstruction = true;
        const auto place = [&](int x, int y)
        {
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                road,
                {{x, y}, 1, 1},
                std::nullopt
            ));
            return map.objectState().completedObjects().back().id;
        };
        const auto id = place(15, 15);
        SettlementGroundCache cache;
        SceneProjection
            p{15.5, 15.5, 32, renderer.outputWidth(), renderer.outputHeight()};
        const auto draw = [&]()
        {
            SceneDrawQueue q;
            cache.begin(map, art);
            PALADIN_CHECK(cache.submit(
                &renderer,
                q,
                p,
                map,
                art,
                *map.objectState().completedObject(id),
                id.value()
            ));
            PALADIN_CHECK(q.size() == 1 && q.items().front().opacity == 255);
            renderer.beginFrame();
            q.render(renderer);
            auto* image = SDL_RenderReadPixels(native, nullptr);
            PALADIN_CHECK(image);
            auto* rgba = SDL_ConvertSurface(image, SDL_PIXELFORMAT_RGBA32);
            PALADIN_CHECK(rgba);
            std::uint64_t hash = 1469598103934665603ull;
            for (int y = 0; y < rgba->h; ++y)
            {
                for (int x = 0; x < rgba->w * 4; ++x)
                {
                    hash = (hash ^ *(static_cast<Uint8*>(rgba->pixels) +
                                     y * rgba->pitch + x)) *
                           1099511628211ull;
                }
            }
            SDL_DestroySurface(rgba);
            SDL_DestroySurface(image);
            return hash;
        };
        const auto cold = draw();
        SDL_Delay(210);
        PALADIN_CHECK(draw() == cold);
        const auto count = cache.buildCount();
        place(25, 25);
        PALADIN_CHECK(draw() == cold && cache.buildCount() == count);
        place(16, 15);
        const auto rebuilt = draw();
        PALADIN_CHECK(cache.buildCount() == count + 1);
        SDL_Delay(210);
        PALADIN_CHECK(draw() == rebuilt);

        // All six land biomes at the same warm temperature must remain
        // distinct. This reproduces the old shared temperature-variant aliasing
        // visually.
        WorldGrid world(60, 24);
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 60; ++x)
            {
                auto& tile = *world.tile({x, y});
                tile.terrain = y < 18 ? TerrainType::Land : TerrainType::Water;
                tile.biome = BiomeType(x / 10);
                tile.temperature = Temperature{.8F};
            }
        }
        WorldGridRenderer terrain;
        Camera2D camera(30, 12);
        camera.setZoom(4);
        TileRenderMetrics metrics{4};
        for (int i = 0; i < 12; ++i)
        {
            renderer.beginFrame();
            terrain.render(renderer, world, camera, metrics, &art);
        }
        if (const char* directory = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            auto* image = SDL_RenderReadPixels(native, nullptr);
            PALADIN_CHECK(image);
            PALADIN_CHECK(SDL_SaveBMP(
                image,
                (std::string(directory) + "/world-all-biomes.bmp").c_str()
            ));
            SDL_DestroySurface(image);
        }
    }
} // namespace Paladin
