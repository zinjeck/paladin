#pragma once
#include "TestFramework.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include <SDL3/SDL.h>
#include <iostream>
namespace Paladin
{
    inline void cityZoomPerformanceChecks(
        Renderer& renderer,
        SDL_Renderer* native
    )
    {
        SettlementGrid grid(576, 576);
        for (int y = 0; y < 576; ++y)
        {
            for (int x = 0; x < 576; ++x)
            {
                auto& t = *grid.tile({x, y});
                t.terrain = TerrainType::Land;
                t.biome = BiomeType::Forest;
                t.temperature = Temperature{.5F};
            }
        }
        SettlementMap
            map(std::move(grid), {200, 200}, 9, 9, 64, 8652264695528414182ULL);
        CityRenderer city;
        map.naturalFeatures().generate(map.grid(), map.generationSeed());
        SettlementObjectPlacementController placement;
        SettlementCommandController command;
        SettlementCitizenState citizens;
        SettlementInspectionController inspection;
        Camera2D camera(288, 288);
        const bool motion = SDL_getenv("PALADIN_CITY_MOTION_REVIEW");
        bool warmed = false;
        for (double zoom : {.75, 1., 1.5, 2., 2.5, 3., .75})
        {
            camera.setZoom(zoom);
            double total = 0, worst = 0, coldWorst = 0;
            std::array<double, 5> stages{};
            const int frames = motion ? 180 : (!warmed ? 600 : 100);
            for (int i = 0; i < frames; ++i)
            {
                if (motion)
                {
                    camera.setPosition(
                        288 + 85 * std::sin(i * .027),
                        288 + 65 * std::cos(i * .023)
                    );
                    camera.setZoom(zoom * (1 + .12 * std::sin(i * .1)));
                }
                else
                {
                    camera.move(.08, .03);
                }
                renderer.beginFrame();
                const auto start = SDL_GetTicksNS();
                city.animationSeconds = i / 30.;
                city.render(
                    renderer,
                    map,
                    camera,
                    TileRenderMetrics{16},
                    placement,
                    command,
                    citizens,
                    inspection,
                    1,
                    23
                );
                SDL_FlushRenderer(native);
                const double ms = (SDL_GetTicksNS() - start) / 1e6;
                if (motion && ms > 20)
                {
                    std::cout << "motion spike zoom=" << zoom << " frame=" << i
                              << " ms=" << ms << " stages=";
                    for (auto t : city.renderTimings)
                    {
                        std::cout << t << ",";
                    }
                    std::cout << std::endl;
                }
                if (i < frames - 60)
                {
                    coldWorst = std::max(coldWorst, ms);
                }
                if (i == 0)
                {
                    std::cout << "first frame zoom=" << zoom << " ms=" << ms
                              << " stages=";
                    for (auto t : city.renderTimings)
                    {
                        std::cout << t << ",";
                    }
                    std::cout << std::endl;
                }
                if (i >= frames - 60)
                {
                    total += ms;
                    worst = std::max(worst, ms);
                    for (int n = 0; n < 5; ++n)
                    {
                        stages[n] += city.renderTimings[n];
                    }
                }
            }
            std::cout << "dense forest zoom=" << zoom << " mean=" << total / 60
                      << " worst=" << worst << " cold_worst=" << coldWorst
                      << " stages=";
            for (auto t : stages)
            {
                std::cout << t / 60 << ",";
            }
            std::cout << std::endl;
            PALADIN_CHECK(total / 60 < (motion ? 12 : 20));
            if (motion)
            {
                PALADIN_CHECK(worst < 20);
                if (warmed)
                {
                    PALADIN_CHECK(coldWorst < 20);
                }
            }
            if (warmed && !motion)
            {
                PALADIN_CHECK(coldWorst < 80);
            }
            warmed = true;
            if (const auto* path = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
            {
                auto* image = SDL_RenderReadPixels(native, nullptr);
                PALADIN_CHECK(
                    image && SDL_SaveBMP(
                                 image,
                                 (std::string(path) + "/dense-city-" +
                                  std::to_string(int(zoom * 16)) + ".bmp")
                                     .c_str()
                             )
                );
                SDL_DestroySurface(image);
                renderer.beginFrame();
                city.render(
                    renderer,
                    map,
                    camera,
                    TileRenderMetrics{16},
                    placement,
                    command,
                    citizens,
                    inspection,
                    1,
                    12
                );
                SDL_FlushRenderer(native);
                image = SDL_RenderReadPixels(native, nullptr);
                PALADIN_CHECK(
                    image && SDL_SaveBMP(
                                 image,
                                 (std::string(path) + "/dense-city-day-" +
                                  std::to_string(int(zoom * 16)) + ".bmp")
                                     .c_str()
                             )
                );
                SDL_DestroySurface(image);
            }
        }
    }
} // namespace Paladin
