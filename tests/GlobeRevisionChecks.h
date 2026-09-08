#pragma once
#include "TestFramework.h"
#include "rendering/GlobeRenderer.h"
#include "rendering/WorldPixelGrid.h"
#include "world/SettlementGrid.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>

namespace Paladin
{
    inline void globeRevisionChecks(
        Renderer& renderer,
        SDL_Renderer* native,
        const SceneSpriteLibrary& art
    )
    {
        std::cout << "globe renderer=" << SDL_GetRendererName(native)
                  << std::endl;
        WorldGenerationSettings settings;
        settings.width = 360;
        settings.height = 240;
        settings.seed = 73517;
        World world(settings);
        std::size_t mountains = 0, hills = 0, land = 0, supported = 0;
        for (int y = 0; y < settings.height; ++y)
        {
            for (int x = 0; x < settings.width; ++x)
            {
                auto& t = *world.grid().tile({x, y});
                land += t.terrain != TerrainType::Water;
                hills += t.biome == BiomeType::Hills;
                if (t.terrain == TerrainType::Mountain)
                {
                    ++mountains;
                    for (int j = -1; j <= 1; ++j)
                    {
                        for (int i = -1; i <= 1; ++i)
                        {
                            if (!i && !j)
                            {
                                continue;
                            }
                            auto* n = world.grid().tile({x + i, y + j});
                            if (n && (n->terrain == TerrainType::Mountain ||
                                      n->biome == BiomeType::Hills))
                            {
                                ++supported;
                                i = 2;
                                j = 2;
                            }
                        }
                    }
                }
            }
        }
        std::cout << "globe generation land=" << land << " hills=" << hills
                  << " mountains=" << mountains << " supported=" << supported
                  << std::endl;
        PALADIN_CHECK(mountains > 20 && hills > 20 && supported == mountains);
        Camera2D camera(settings.width * .5, settings.height * .5);
        GlobeRenderer globe;
        const auto draw = [&]()
        {
            renderer.beginFrame();
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            {
                WorldPixelScene pixels(
                    renderer,
                    view.radius * 6.283185307 / settings.width
                );
                globe.render(renderer, world, camera, art, {}, {});
            }
            SDL_FlushRenderer(native);
        };
        const auto until = SDL_GetTicks() + 15000;
        do
        {
            draw();
            SDL_Delay(1);
        } while (!globe.detailReady() && SDL_GetTicks() < until);
        PALADIN_CHECK(globe.detailReady());
        const auto save = [&](const char* name)
        {
            if (const auto* path = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
            {
                auto* surface = SDL_RenderReadPixels(native, nullptr);
                PALADIN_CHECK(
                    surface &&
                    SDL_SaveBMP(
                        surface,
                        (std::string(path) + "/" + name + ".bmp").c_str()
                    )
                );
                SDL_DestroySurface(surface);
            }
        };
        draw();
        save("globe-whole");
        world.time().advanceMinutes(360);
        draw();
        save("globe-noon");
        world.time().advanceMinutes(720);
        draw();
        save("globe-midnight");
        world.time().advanceMinutes(720);
        auto view = GlobeView::from(
            camera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight()
        );
        PALADIN_CHECK(!view.pick(view.cx + view.radius + 1, view.cy));
        for (double u : {.01, .25, .5, .99})
        {
            for (double v : {.1, .5, .9})
            {
                camera.setPosition(u * settings.width, v * settings.height);
                view = GlobeView::from(
                    camera,
                    world.grid(),
                    renderer.outputWidth(),
                    renderer.outputHeight()
                );
                auto p = view.project(u, v);
                auto hit = view.pick(p.x, p.y);
                PALADIN_CHECK(
                    p.z > .999 && hit && std::abs(hit->u - u) < 1e-9 &&
                    std::abs(hit->v - v) < 1e-9
                );
            }
        }
        for (int y = 40; y < settings.height - 40; ++y)
        {
            for (int x = 40; x < settings.width - 40; ++x)
            {
                if (world.grid().tile({x, y})->terrain == TerrainType::Mountain)
                {
                    camera.setPosition(x + .5, y + .5);
                    x = settings.width;
                    y = settings.height;
                }
            }
        }
        camera.setZoom(3);
        for (int i = 0; i < 120; ++i)
        {
            draw();
        }
        save("globe-regional");
        camera.setZoom(7);
        const auto detailDeadline = SDL_GetTicks() + 60000;
        while (!globe.fullDetailReady() && SDL_GetTicks() < detailDeadline)
        {
            draw();
            SDL_Delay(1);
        }
        PALADIN_CHECK(globe.fullDetailReady());
        draw();
        save("globe-close");
        camera.setZoom(16);
        draw();
        save("globe-relief-near");
        double total = 0, worst = 0;
        for (int i = 0; i < 90; ++i)
        {
            camera.setZoom(.7 + 6. * (1 + std::sin(i * .11)) * .5);
            camera.move(.7, .05);
            const auto before = SDL_GetTicksNS();
            draw();
            const double ms = (SDL_GetTicksNS() - before) / 1e6;
            total += ms;
            worst = std::max(worst, ms);
        }
        PALADIN_CHECK(globe.atlasBuilds == 1);
        PALADIN_CHECK(worst < 80);
        SettlementGrid coast(128, 128);
        for (int y = 0; y < 128; ++y)
        {
            for (int x = 0; x < 128; ++x)
            {
                auto& t = *coast.tile({x, y});
                t.terrain = x < 80 + std::sin(y * .12) * 7 ? TerrainType::Land
                                                           : TerrainType::Water;
                t.biome = t.terrain == TerrainType::Land ? BiomeType::Plain
                                                         : BiomeType::Ocean;
            }
        }
        coast.classifyCoast(817);
        WorldGridRenderer city;
        Camera2D cityCamera(79, 64);
        const auto cityDraw = [&]()
        {
            renderer.beginFrame();
            {
                WorldPixelScene pixels(renderer, 16 * cityCamera.zoom());
                city.render(
                    renderer,
                    coast,
                    cityCamera,
                    TileRenderMetrics{16},
                    &art
                );
            }
            SDL_FlushRenderer(native);
        };
        for (double zoom : {.5, 1., 4.})
        {
            cityCamera.setZoom(zoom);
            for (int i = 0; i < 80; ++i)
            {
                cityDraw();
                SDL_Delay(1);
            }
            save(
                zoom == .5  ? "city-coast-far"
                : zoom == 1 ? "city-coast-normal"
                            : "city-coast-close"
            );
        }
        double cityWorst = 0, cityTotal = 0;
        for (int i = 0; i < 60; ++i)
        {
            cityCamera.setZoom(.5 + 3.5 * (1 + std::sin(i * .16)) * .5);
            const auto start = SDL_GetTicksNS();
            cityDraw();
            const double elapsed = (SDL_GetTicksNS() - start) / 1e6;
            cityTotal += elapsed;
            cityWorst = std::max(cityWorst, elapsed);
        }
        std::cout << "city coast zoom mean_ms=" << cityTotal / 60
                  << " worst_ms=" << cityWorst << std::endl;
        std::cout << "globe warm zoom/rotation mean_ms=" << total / 90
                  << " worst_ms=" << worst
                  << " atlas_builds=" << globe.atlasBuilds << std::endl;
        if (const auto* path = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            std::ofstream(std::string(path) + "/globe-metrics.txt")
                << "land " << land << "\nhills " << hills << "\nmountains "
                << mountains << "\nmean_ms " << total / 90 << "\nworst_ms "
                << worst << "\natlas_builds " << globe.atlasBuilds << "\n";
        }
    }
} // namespace Paladin
