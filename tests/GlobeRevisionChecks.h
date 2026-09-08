#pragma once
#include "TestFramework.h"
#include "interaction/GlobeCameraNavigation.h"
#include "rendering/GlobeRenderer.h"
#include "rendering/WorldPixelGrid.h"
#include "rendering/WorldRenderer.h"
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
                                      n->relief == ReliefType::Hills))
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
        // Screen-relative motion remains continuous through both poles.
        const int vw = renderer.outputWidth(), vh = renderer.outputHeight();
        Camera2D orbit(settings.width * .5, settings.height * .5);
        orbit.setPlanetRotation({}, settings.width, settings.height);
        const double radius =
            GlobeView::from(orbit, world.grid(), vw, vh).radius;
        double minV = 1, maxV = 0;
        for (int i = 0; i < 720; ++i)
        {
            GlobeCameraNavigation::pan(
                orbit,
                world.grid(),
                vw,
                vh,
                0,
                -1,
                6.283185307179586 * radius / 720
            );
            const auto ov = GlobeView::from(orbit, world.grid(), vw, vh);
            const auto uv = ov.pick(ov.cx, ov.cy);
            PALADIN_CHECK(uv);
            minV = std::min(minV, uv->v);
            maxV = std::max(maxV, uv->v);
            PALADIN_CHECK(
                std::isfinite(orbit.tileX()) && std::isfinite(orbit.tileY())
            );
        }
        PALADIN_CHECK(minV < 1e-6 && maxV > 1 - 1e-6);
        PALADIN_CHECK(orbit.planetRotation()->apply({0, 0, 1}).z > .999999);
        for (double roll : {0., .7, 2., 3.141592653589793})
        {
            orbit.setPlanetRotation(
                PlanetRotation::axis(0, 0, 1, roll),
                settings.width,
                settings.height
            );
            auto ov = GlobeView::from(orbit, world.grid(), vw, vh);
            auto origin = ov.pick(ov.cx, ov.cy);
            PALADIN_CHECK(origin);
            GlobeCameraNavigation::pan(orbit, world.grid(), vw, vh, 1, 0, 10);
            auto moved = GlobeView::from(orbit, world.grid(), vw, vh)
                             .project(origin->u, origin->v);
            PALADIN_CHECK(moved.x < ov.cx && std::abs(moved.y - ov.cy) < 1e-8);
            auto before = GlobeView::from(orbit, world.grid(), vw, vh);
            const double x0 = before.cx + radius * .25,
                         y0 = before.cy - radius * .2,
                         x1 = before.cx - radius * .15,
                         y1 = before.cy + radius * .25;
            auto grabbed = before.pick(x0, y0);
            PALADIN_CHECK(grabbed);
            GlobeCameraNavigation::drag(
                orbit,
                world.grid(),
                vw,
                vh,
                x0,
                y0,
                x1,
                y1
            );
            auto after = GlobeView::from(orbit, world.grid(), vw, vh)
                             .project(grabbed->u, grabbed->v);
            PALADIN_CHECK(std::hypot(after.x - x1, after.y - y1) < 1e-7);
        }
        const auto mb = WorldMapNavigation::mapBounds(vw, vh);
        const auto middle = WorldMapNavigation::minimapPoint(
            mb,
            mb.x + mb.width * .5,
            mb.y + mb.height * .5
        );
        PALADIN_CHECK(
            std::abs(middle.u - .5) < 1e-6 && std::abs(middle.v - .5) < 1e-6
        );
        WorldRenderer modes;
        modes.globeEnabled = true;
        for (int i = 0; i < 20; ++i)
        {
            const auto old =
                GlobeView::from(orbit, world.grid(), vw, vh).orientation();
            const double x = orbit.tileX(), y = orbit.tileY(),
                         zoom = orbit.zoom();
            modes.toggleProjection(
                orbit,
                world.grid(),
                vw,
                vh,
                TileRenderMetrics{16}
            );
            PALADIN_CHECK(!modes.globeEnabled && !orbit.planetRotation());
            PALADIN_CHECK(
                std::abs(orbit.tileX() - x) < 1e-8 &&
                std::abs(orbit.tileY() - y) < 1e-8
            );
            modes.toggleProjection(
                orbit,
                world.grid(),
                vw,
                vh,
                TileRenderMetrics{16}
            );
            auto relative =
                GlobeView::from(orbit, world.grid(), vw, vh).orientation() *
                old.inverse();
            PALADIN_CHECK(
                std::abs(relative.w) > .999999999 &&
                std::abs(orbit.zoom() - zoom) < 1e-8
            );
        }
        WorldMapNavigation::focus(
            orbit,
            world.grid(),
            vw,
            vh,
            true,
            {.999, .001}
        );
        PALADIN_CHECK(
            std::abs(orbit.tileX() / settings.width - .999) < 1e-8 &&
            std::abs(orbit.tileY() / settings.height - .001) < 1e-8
        );
        Camera2D flat(-1, settings.height * .5);
        auto seam = WorldMapNavigation::pick(
            flat,
            world.grid(),
            vw,
            vh,
            16,
            false,
            vw * .5,
            vh * .5
        );
        PALADIN_CHECK(seam && seam->u > .99);
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
        // Off-center picking must undo the axial tilt as well as yaw/pitch.
        for (double u : {.4, .5, .6})
        {
            for (double v : {.35, .5, .65})
            {
                const auto point = view.project(u, v);
                const auto hit = view.pick(point.x, point.y);
                PALADIN_CHECK(
                    hit && std::abs(hit->u - u) < 1e-9 &&
                    std::abs(hit->v - v) < 1e-9
                );
            }
        }
        const auto north = view.project(.5, .25);
        PALADIN_CHECK(north.x < view.cx); // 23.5 degree tilted northern axis
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
        std::cout << "globe measured mean=" << total / 90 << " worst=" << worst
                  << std::endl;
        PALADIN_CHECK(worst < 80);
        // Switching projections reuses atlases. Both views refresh on one edit.
        GrayUiRenderer navigationUi;
        modes.globeEnabled = true;
        orbit.setZoom(1);
        const auto modeDraw = [&]()
        {
            renderer.beginFrame();
            modes.render(renderer, world, orbit, TileRenderMetrics{16});
            modes.renderNavigator(
                renderer,
                world,
                orbit,
                TileRenderMetrics{16},
                navigationUi
            );
            SDL_FlushRenderer(native);
        };
        auto deadline = SDL_GetTicks() + 15000;
        do
        {
            modeDraw();
            SDL_Delay(1);
        } while (!modes.terrainDetailReady() && SDL_GetTicks() < deadline);
        PALADIN_CHECK(modes.terrainDetailReady());
        save("globe-navigator");
        for (int i = 0; i < 20; ++i)
        {
            modes.toggleProjection(
                orbit,
                world.grid(),
                vw,
                vh,
                TileRenderMetrics{16}
            );
            modeDraw();
        }
        PALADIN_CHECK(modes.terrainAtlasBuilds() == 1);
        modes.toggleProjection(
            orbit,
            world.grid(),
            vw,
            vh,
            TileRenderMetrics{16}
        );
        modeDraw();
        save("flat-navigator");
        const auto revision = world.grid().revision();
        auto changed = *world.grid().tile({10, 10});
        changed.terrain = TerrainType::Water;
        changed.biome = BiomeType::Ocean;
        PALADIN_CHECK(world.grid().setTile({10, 10}, changed));
        PALADIN_CHECK(world.grid().revision() == revision + 1);
        modeDraw();
        PALADIN_CHECK(modes.terrainAtlasBuilds() == 2);
        modes.toggleProjection(
            orbit,
            world.grid(),
            vw,
            vh,
            TileRenderMetrics{16}
        );
        modeDraw();
        PALADIN_CHECK(modes.terrainAtlasBuilds() == 2);
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
