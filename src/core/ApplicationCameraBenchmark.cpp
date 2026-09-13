#include "core/Application.h"
#include "core/SimulationClock.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "simulation/Simulation.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace Paladin
{
    int Application::runCameraBenchmark()
    {
        SDL_HideWindow(window_->nativeHandle());
        SDL_SetRenderVSync(SDL_GetRenderer(window_->nativeHandle()), 0);
        startWorldSession();
        auto& world = simulation_->world();
        SettlementId capital;
        for (int y = 20; y < world.grid().height() - 20 && !capital; ++y)
        {
            for (int x = 20; x < world.grid().width() - 20 && !capital; ++x)
            {
                if (world.canFoundSettlementAt(
                        {x, y},
                        simulation_->playerRealmId()
                    ))
                {
                    capital = simulation_->foundPlayerCapital(
                        {x, y},
                        {"Camera Review",
                         "Review Folk",
                         "Review City",
                         {},
                         "tribal",
                         {},
                         "Ronan"}
                    );
                }
            }
        }
        if (!capital)
        {
            return 2;
        }
        enterPresentedSettlement();
        if (screen_ != Screen::City)
        {
            return 3;
        }
        const auto* map = simulation_->settlementMap(capital);
        const double centerX = map->grid().width() * .5,
                     centerY = map->grid().height() * .5;
        std::cout << "Actual game camera benchmark: renderer="
                  << SDL_GetRendererName(
                         SDL_GetRenderer(window_->nativeHandle())
                     )
                  << " local_map=" << map->grid().width() << 'x'
                  << map->grid().height() << " realms=" << world.realmCount()
                  << " settlements=" << world.settlementCount() << '\n';
        simulationClock_->setPaused(false);
        for (double speed : {1., 5.})
        {
            for (bool scroll : {false, true})
            {
                simulationClock_->setSpeedMultiplier(speed);
                std::vector<double> samples;
                for (int i = 0; i < 120; ++i)
                {
                    const auto start = SDL_GetTicksNS();
                    // Cold and changing views, with no stationary prewarm
                    // excluded.
                    const double pixels =
                        scroll ? 48
                               : 2 + 62 * (.5 -
                                           .5 * std::cos(
                                                    i * 6.283185307179586 / 119
                                                ));
                    camera_->setZoom(pixels / tileRenderMetrics_->tilePixels);
                    camera_->setPosition(
                        centerX + (scroll ? i * 1.5 - 90 : 0),
                        centerY + (scroll ? 24 * std::sin(i * .09) : 0)
                    );
                    clampCameraToWorld();
                    simulationClock_->beginFrame();
                    layoutFrame();
                    renderFrame();
                    samples.push_back(double(SDL_GetTicksNS() - start) / 1e6);
                    SDL_PumpEvents();
                }
                double total = 0;
                for (double ms : samples)
                {
                    total += ms;
                }
                std::sort(samples.begin(), samples.end());
                std::cout << (scroll ? "fast_scroll" : "continuous_zoom")
                          << " speed=" << speed << " frames=" << samples.size()
                          << " mean_ms=" << total / samples.size() << " p95_ms="
                          << samples[std::size_t(samples.size() * .95)]
                          << " worst_ms=" << samples.back() << std::endl;
            }
        }
        return 0;
    }
} // namespace Paladin
