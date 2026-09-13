#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "simulation/systems/SettlementPopulationSystem.h"
#include "ui/LedgerPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#ifdef PALADIN_SOURCE_ART
#include <SDL3_image/SDL_image.h>
#endif
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace Paladin
{
    namespace
    {
#ifdef PALADIN_SOURCE_ART
        constexpr const char* BenchmarkImageExtension = ".png";
        bool saveBenchmarkImage(SDL_Surface* image, const char* path)
        {
            return IMG_SavePNG(image, path);
        }
#else
        constexpr const char* BenchmarkImageExtension = ".bmp";
        bool saveBenchmarkImage(SDL_Surface* image, const char* path)
        {
            return SDL_SaveBMP(image, path);
        }
#endif
    }
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

    int Application::runWorldBenchmark()
    {
        SDL_HideWindow(window_->nativeHandle());
        SDL_SetRenderVSync(SDL_GetRenderer(window_->nativeHandle()), 0);
        if (const char* width = SDL_getenv("PALADIN_WORLD_BENCHMARK_WIDTH"))
        {
            const char* height = SDL_getenv("PALADIN_WORLD_BENCHMARK_HEIGHT");
            if (height && SDL_atoi(width) >= 256 && SDL_atoi(height) >= 256 &&
                SDL_atoi(width) <= 8192 && SDL_atoi(height) <= 8192)
            {
                SDL_SetWindowSize(
                    window_->nativeHandle(),
                    SDL_atoi(width),
                    SDL_atoi(height)
                );
                SDL_SyncWindow(window_->nativeHandle());
            }
        }
        startWorldSession();
        WorldGenerationSettings settings;
        settings.seed = 73517;
        simulation_ = std::make_unique<Simulation>(settings);
        auto& world = simulation_->world();
        WorldTilePosition center{settings.width / 2, settings.height / 2};
        double closest = 1e20;
        for (const auto& city : world.settlements())
        {
            const double distance = std::hypot(
                city.position().x - settings.width * .5,
                city.position().y - settings.height * .5
            );
            if (distance < closest)
            {
                closest = distance;
                center = city.position();
            }
        }
        GlobeCameraNavigation::focusNorthUp(*camera_, world.grid(), center);
        // Same completed initial loading boundary as interactive play; local
        // political detail is intentionally cold on the first sweep.
        const auto preparationStart = SDL_GetTicksNS();
        for (;;)
        {
            renderer_->beginFrame();
            const bool prepared =
                worldRenderer_->prepareTerrain(*renderer_, world);
            // Match the real loading screen: submit every frame instead of
            // leaving thousands of clears/uploads queued for the first timed
            // present after an asynchronous terrain build.
            renderer_->endFrame();
            if (prepared)
            {
                break;
            }
            SDL_PumpEvents();
            SDL_Delay(1);
        }
        const double preparationMs =
            (SDL_GetTicksNS() - preparationStart) / 1e6;
        worldRenderer_->profileRendering = true;
        std::ofstream csv;
        if (const char* file = SDL_getenv("PALADIN_WORLD_PROFILE"))
        {
            csv.open(file);
        }
        if (csv)
        {
            csv << "pass,frame,zoom,frame_ms,setup,sphere,sphere_politics,flat,"
                   "foliage,flat_politics,sun,markers\n";
        }
        std::cout << "World zoom renderer="
                  << SDL_GetRendererName(
                         SDL_GetRenderer(window_->nativeHandle())
                     )
                  << " seed=" << settings.seed << " size=" << settings.width
                  << 'x' << settings.height
                  << " viewport=" << renderer_->outputWidth() << 'x'
                  << renderer_->outputHeight()
                  << " realms=" << world.realmCount()
                  << " settlements=" << world.settlementCount()
                  << " preparation_ms=" << preparationMs << std::endl;
        for (int pass = 0; pass < 3; ++pass)
        {
            worldRenderer_->setMapMode(
                pass == 2 ? WorldMapMode::Terrain : WorldMapMode::Political
            );
            std::vector<double> samples;
            for (int i = 0; i < 180; ++i)
            {
                // Logarithmic in/out sweep crosses every presentation tier and
                // returns to the same resident pages with unchanged astronomy.
                const double zoom = std::exp(
                    std::log(.6) +
                    (.5 - .5 * std::cos(i * 6.283185307179586 / 179)) *
                        std::log(100.)
                );
                camera_->setWorldZoom(zoom);
                worldRenderer_->animationSeconds = i / 60.;
                const auto start = SDL_GetTicksNS();
                layoutFrame();
                renderFrame();
                const double ms = (SDL_GetTicksNS() - start) / 1e6;
                samples.push_back(ms);
                if (csv)
                {
                    csv << pass << ',' << i << ',' << zoom << ',' << ms;
                    for (double t : worldRenderer_->renderTimings)
                    {
                        csv << ',' << t;
                    }
                    csv << '\n';
                }
                if (const char* dir = SDL_getenv("PALADIN_WORLD_CAPTURES");
                    dir && pass == 1 &&
                    (i == 0 || i == 36 || i == 52 || i == 66 || i == 89))
                {
                    auto* image = SDL_RenderReadPixels(
                        SDL_GetRenderer(window_->nativeHandle()),
                        nullptr
                    );
                    if (image)
                    {
                        std::filesystem::create_directories(dir);
                        saveBenchmarkImage(
                            image,
                            (std::filesystem::path(dir) /
                             ("world-zoom-" + std::to_string(i) + BenchmarkImageExtension))
                                .string()
                                .c_str()
                        );
                        SDL_DestroySurface(image);
                    }
                }
                SDL_PumpEvents();
            }
            double total = 0;
            int freezes = 0;
            for (double ms : samples)
            {
                total += ms;
                freezes += ms > 50;
            }
            std::sort(samples.begin(), samples.end());
            std::cout << "pass=" << pass
                      << " mean_ms=" << total / samples.size()
                      << " p95_ms=" << samples[171]
                      << " worst_ms=" << samples.back()
                      << " over_50_ms=" << freezes << std::endl;
        }

        if (const char* dir = SDL_getenv("PALADIN_WORLD_CAPTURES"))
        {
            // Captures are separate from measured sweeps. Let every requested
            // view finish its bounded political work before comparing pixels.
            worldRenderer_->setMapMode(WorldMapMode::Political);
            std::filesystem::create_directories(dir);
            for (int i : {0, 36, 52, 66, 89})
            {
                camera_->setWorldZoom(
                    std::exp(
                        std::log(.6) +
                        (.5 - .5 * std::cos(i * 6.283185307179586 / 179)) *
                            std::log(100.)
                    )
                );
                worldRenderer_->animationSeconds = i / 60.;
                const auto settleStart = SDL_GetTicksNS();
                int frames = 0;
                do
                {
                    layoutFrame();
                    renderFrame();
                    SDL_PumpEvents();
                    ++frames;
                } while (worldRenderer_->politicalWorkPending() &&
                         SDL_GetTicksNS() - settleStart < 30'000'000'000ULL);
                if (worldRenderer_->politicalWorkPending())
                {
                    std::cerr << "Political capture did not settle at frame "
                              << i << '\n';
                    return 5;
                }
                // Read the completed backbuffer before Present rotates the
                // D3D swap chain. Reading after Present can return an older
                // buffer (including the preceding terrain-only sweep).
                layoutFrame();
                renderer_->beginFrame();
                renderWorldScreen();
                ledgerPanel_->render(*renderer_, *grayUiRenderer_);
                renderDebug();
                auto* image = SDL_RenderReadPixels(
                    SDL_GetRenderer(window_->nativeHandle()),
                    nullptr
                );
                renderer_->endFrame();
                if (!image)
                {
                    return 6;
                }
                const auto file =
                    std::filesystem::path(dir) /
                    ("world-zoom-" + std::to_string(i) + "-settled" + BenchmarkImageExtension);
                const bool saved = saveBenchmarkImage(image, file.string().c_str());
                SDL_DestroySurface(image);
                if (!saved)
                {
                    return 6;
                }
                std::cout << "capture=" << i << " settle_frames=" << frames
                          << " settle_ms="
                          << (SDL_GetTicksNS() - settleStart) / 1e6
                          << std::endl;
            }
        }

        if (const char* live = SDL_getenv("PALADIN_WORLD_LIVE_BENCHMARK");
            live && std::string_view(live) == "1")
        {
            // Keep the three camera-only sweeps above identical. These extra
            // sweeps exercise real strategic demography on the same seeded AI
            // world without creating or simulating a detailed player city.
            std::ofstream liveCsv;
            if (const char* file = SDL_getenv("PALADIN_WORLD_LIVE_PROFILE"))
            {
                liveCsv.open(file);
            }
            else if (const char* file = SDL_getenv("PALADIN_WORLD_PROFILE"))
            {
                liveCsv.open(std::string(file) + ".live.csv");
            }
            if (liveCsv)
            {
                liveCsv << "pass,frame,zoom,game_minutes,residents,demographic_"
                           "version,influence_revision,influence_changed,frame_"
                           "ms,setup,sphere,sphere_politics,flat,foliage,flat_"
                           "politics,sun,markers\n";
            }

            std::uint64_t previousRevision = world.tribalInfluence().revision();
            std::vector<SettlementSimulationStep> steps;
            for (auto& city : world.settlements())
            {
                city.simulationState().population().setRates(
                    {0.0, 0.0, 1.0, 0.0, 0.0}
                );
                steps.push_back(
                    {city.id(),
                     SettlementSimulationTier::Strategic,
                     SettlementSimulationResolution::StrategicAggregate,
                     1440}
                );
            }
            const auto totalResidents = [&]
            {
                std::uint64_t total = 0;
                for (const auto& city : world.settlements())
                {
                    total += city.population();
                }
                return total;
            };
            SettlementPopulationSystem populationSystem;
            worldRenderer_->setMapMode(WorldMapMode::Political);
            for (int pass = 3; pass < 5; ++pass)
            {
                const auto openingResidents = totalResidents();
                std::vector<double> samples;
                int revisionChanges = 0;
                for (int i = 0; i < 180; ++i)
                {
                    const double zoom = std::exp(
                        std::log(.6) +
                        (.5 - .5 * std::cos(i * 6.283185307179586 / 179)) *
                            std::log(100.)
                    );
                    camera_->setWorldZoom(zoom);
                    worldRenderer_->animationSeconds = i / 60.;
                    // A day's fractional migration changes no resident count.
                    // Four annual ticks in the final sweep add exactly one
                    // resident per AI settlement and require a real refresh.
                    const std::uint64_t minutes =
                        pass == 4 && i % 45 == 0 ? 365 * 1440 : 1440;
                    for (auto& step : steps)
                    {
                        step.gameMinutes = minutes;
                    }
                    const auto start = SDL_GetTicksNS();
                    world.time().advanceMinutes(minutes);
                    populationSystem.tick(world, {minutes, steps});
                    layoutFrame();
                    renderFrame();
                    const double ms = (SDL_GetTicksNS() - start) / 1e6;
                    samples.push_back(ms);
                    const auto revision = world.tribalInfluence().revision();
                    const bool changed = revision != previousRevision;
                    revisionChanges += changed;
                    previousRevision = revision;
                    if (liveCsv)
                    {
                        std::uint64_t demographicVersion = 0;
                        for (const auto& city : world.settlements())
                        {
                            demographicVersion +=
                                city.simulationState().population().version();
                        }
                        liveCsv << pass << ',' << i << ',' << zoom << ','
                                << minutes << ',' << totalResidents() << ','
                                << demographicVersion << ',' << revision << ','
                                << changed << ',' << ms;
                        for (double timing : worldRenderer_->renderTimings)
                        {
                            liveCsv << ',' << timing;
                        }
                        liveCsv << '\n';
                    }
                    SDL_PumpEvents();
                }
                double total = 0;
                int freezes = 0;
                for (double ms : samples)
                {
                    total += ms;
                    freezes += ms > 50;
                }
                std::sort(samples.begin(), samples.end());
                const auto gainedResidents =
                    totalResidents() - openingResidents;
                std::cout << "pass=" << pass << " scenario="
                          << (pass == 3 ? "fractional_demography"
                                        : "resident_growth")
                          << " mean_ms=" << total / samples.size()
                          << " p95_ms=" << samples[171]
                          << " worst_ms=" << samples.back()
                          << " over_50_ms=" << freezes
                          << " influence_changes=" << revisionChanges
                          << " residents_added=" << gainedResidents
                          << std::endl;
                const auto expectedGrowth = pass == 3 ? 0 : steps.size() * 4;
                const int expectedRevisions = pass == 3 ? 0 : 4;
                if (gainedResidents != expectedGrowth ||
                    revisionChanges != expectedRevisions)
                {
                    std::cerr << "Unexpected live population/influence change "
                                 "cadence\n";
                    return 4;
                }
            }
        }
        return 0;
    }
} // namespace Paladin
