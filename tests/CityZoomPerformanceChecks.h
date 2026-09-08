#pragma once
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/CityRenderer.h"
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
        SettlementObjectPlacementController placement;
        SettlementCommandController command;
        SettlementCitizenState citizens;
        SettlementInspectionController inspection;
        Camera2D camera(288, 288);
        for (double zoom : {.25, .5, .75, 1., 1.5, 2.})
        {
            camera.setZoom(zoom);
            double total = 0, worst = 0;
            std::array<double, 5> stages{};
            for (int i = 0; i < 100; ++i)
            {
                camera.move(.08, .03);
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
                if (i >= 40)
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
                      << " worst=" << worst << " stages=";
            for (auto t : stages)
            {
                std::cout << t / 60 << ",";
            }
            std::cout << std::endl;
        }
    }
} // namespace Paladin
