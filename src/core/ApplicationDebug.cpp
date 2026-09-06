#include "core/Application.h"
#include "core/SimulationClock.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "debug/ConsoleCommand.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/DebugConsole.h"
#include "ui/GrayUiRenderer.h"
#include "ui/SettlementInspectionPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <SDL3/SDL.h>
#include <iomanip>
#include <sstream>

namespace Paladin
{
    void Application::executeConsoleCommand(std::string_view text)
    {
        const auto command = parseConsoleCommand(text);
        if (command.kind == ConsoleCommandKind::Empty)
        {
            return;
        }
        if (command.kind == ConsoleCommandKind::Stats)
        {
            debugConsole_->showStats();
            nextStatsRefresh_ = 0;
            return;
        }
        if (command.kind == ConsoleCommandKind::Invalid)
        {
            debugConsole_->print(command.error);
            return;
        }
        // Presented settlement is the active player context, independent of
        // view/detailed tier.
        auto id = simulation_->presentedSettlementId();
        if (!id)
        {
            if (const auto* realm =
                    simulation_->world().realm(simulation_->playerRealmId()))
            {
                id = realm->capitalSettlementId();
            }
        }
        auto* settlement = simulation_->world().settlement(id);
        if (!settlement ||
            settlement->ownerRealmId() != simulation_->playerRealmId())
        {
            debugConsole_->print(
                "No active player settlement. Found a settlement first."
            );
            return;
        }
        try
        {
            if (!settlement->simulationState().spawnCitizens(command.count))
            {
                debugConsole_->print("Unable to spawn citizens.");
                return;
            }
            if (auto* map = simulation_->settlementMap(id))
            {
                map->employment().record(
                    simulation_->world().time().totalGameMinutes(),
                    settlement->simulationState().citizens()
                );
            }
            debugConsole_->print(
                "Successfully spawned " + std::to_string(command.count) +
                (command.count == 1 ? " citizen" : " citizens") +
                " in settlement #" + std::to_string(id.value()) + "."
            );
            nextStatsRefresh_ = 0;
        }
        catch (const std::bad_alloc&)
        {
            debugConsole_->print(
                "Unable to spawn citizens: insufficient memory."
            );
            return;
        }
        if (auto* map = simulation_->settlementMap(id))
        {
            try
            {
                settlement->simulationState()
                    .citizens()
                    .placeUnpositionedCitizens(*map);
                map->activities.synchronizeHomes(
                    *map,
                    settlement->simulationState().citizens()
                );
            }
            catch (const std::bad_alloc&)
            {
                debugConsole_->print(
                    "Citizens created; map placement deferred due "
                    "to memory pressure."
                );
            }
        }
    }

    void Application::renderDebug()
    {
        if (debugConsole_->statsVisible() &&
            SDL_GetTicks() >= nextStatsRefresh_)
        {
            nextStatsRefresh_ = SDL_GetTicks() + 200;
            const auto& world = simulation_->world();
            std::ostringstream s;
            s << std::fixed << std::setprecision(3);
            s << "World time: " << world.time().totalGameMinutes()
              << " minutes\nClock: "
              << (simulationClock_->isPaused() ? "Paused" : "Running")
              << " | Speed: " << simulationClock_->speedMultiplier()
              << "x\nTick: " << simulation_->tickCount()
              << " | Backlog: " << simulationClock_->backlogTicks()
              << "\nBacklog limit hits: " << simulationClock_->limitHits
              << " | Discarded: " << simulationClock_->discardedSeconds << " s"
              << "\nFrame: " << simulationClock_->frameDeltaSeconds() * 1000
              << " ms | FPS: "
              << (simulationClock_->frameDeltaSeconds() > 0
                      ? 1 / simulationClock_->frameDeltaSeconds()
                      : 0)
              << "\nTiming ms: last / avg / p95 / max (120 samples)\nTotal: "
              << simulation_->tickTiming.text()
              << "\nCitizens: " << simulation_->citizenTiming.text()
              << "\nAggregate: " << simulation_->aggregateTiming.text() << "\n"
              << simulation_->systemTimingText() << "Minutes/tick: "
              << simulation_->gameMinutesPerTick(
                     simulationClock_->fixedDeltaSeconds()
                 )
              << "\nSlow ticks >= 50 ms: " << simulation_->tickTiming.slow
              << "\nScene: " << (screen_ == Screen::City ? "City" : "World")
              << " | Seed: " << world.generationSeed()
              << "\nSettlements: " << world.settlementCount()
              << " | Detailed: #"
              << simulation_->detailedSimulationSettlementId().value();
            auto id = simulation_->presentedSettlementId();
            if (!id)
            {
                if (const auto* p = world.realm(simulation_->playerRealmId()))
                {
                    id = p->capitalSettlementId();
                }
            }
            if (const auto* settlement = world.settlement(id))
            {
                const auto& state = settlement->simulationState();
                const auto& citizens = state.citizens();
                const auto* map = simulation_->settlementMap(id);
                std::size_t moving = 0, working = 0, paths = 0, invalidJobs = 0;
                for (const auto& c : citizens.citizens())
                {
                    moving += !c.path.empty();
                    working += c.activity == CitizenActivity::AtWork;
                    paths += c.path.size();
                    invalidJobs +=
                        c.workplaceId &&
                        (!map || !map->employment().workplace(c.workplaceId));
                }
                s << "\nActive settlement: #" << id.value() << " | Owner: #"
                  << settlement->ownerRealmId().value()
                  << "\nTier: " << int(state.simulationTier())
                  << " | Pending minutes: " << state.pendingSimulationMinutes()
                  << "\nPopulation: " << state.population().residents()
                  << " | Citizens: " << citizens.citizens().size()
                  << "\nMoving: " << moving << " | At work: " << working
                  << " | Path steps: " << paths
                  << "\nValidation - orphaned jobs: " << invalidJobs;
                if (map)
                {
                    s << "\nLocal seed: " << map->generationSeed()
                      << " | Size: " << map->grid().width() << " x "
                      << map->grid().height() << "\nObjects: "
                      << map->objectState().completedObjects().size()
                      << " | Construction: "
                      << map->objectState().constructionSites().size()
                      << "\nCommands: " << map->commandState().commands().size()
                      << " | Workplaces: "
                      << map->employment().workplaces().size()
                      << "\nUnemployed: "
                      << map->employment().unemployed(citizens);
                }
                const auto& nav = citizens.navigationDiagnostics();
                s << "\nNavigation requests: " << nav.requests
                  << " | Failed: " << nav.failures
                  << "\nExpanded: " << nav.expandedNodes
                  << " | Candidates: " << nav.candidates
                  << " | Cost: " << nav.lastCost
                  << "\nNavigation ms: " << nav.timing.text();
                s << "\nResources (secured):";
                for (const auto& entry : state.stockpile().entries())
                {
                    s << "\n" << entry.resourceId << ": " << entry.amount;
                }
                s << "\nLoose/physical resource conservation: not implemented";
            }
            float mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            const auto pixels =
                tileRenderMetrics_->scaledTilePixels(camera_->zoom());
            const int x = int(std::floor(
                camera_->tileX() + (mx - renderer_->outputWidth() * .5) / pixels
            ));
            const int y = int(std::floor(
                camera_->tileY() +
                (my - renderer_->outputHeight() * .5) / pixels
            ));
            const WorldTile* tile = nullptr;
            if (screen_ == Screen::City)
            {
                if (const auto* map =
                        simulation_->settlementMap(activeCitySettlementId_))
                {
                    tile = map->grid().tile({x, y});
                }
            }
            else
            {
                tile = world.grid().tile({x, y});
            }
            s << "\nCursor tile: " << x << ", " << y
              << " | Zoom: " << camera_->zoom();
            if (tile)
            {
                constexpr const char* terrains[] =
                    {"Land", "Water", "Mountain"};
                constexpr const char* biomes[] = {
                    "Plain",
                    "Forest",
                    "Jungle",
                    "Desert",
                    "Tundra",
                    "Taiga",
                    "Ocean"
                };
                s << "\nTerrain: " << terrains[int(tile->terrain)]
                  << " | Biome: " << biomes[int(tile->biome)]
                  << "\nElevation: " << tile->elevation.value()
                  << " | Temperature: " << tile->temperature.value()
                  << "\nPrecipitation: " << tile->rainfall.value();
            }
            if (screen_ == Screen::City)
            {
                if (const auto* map =
                        simulation_->settlementMap(activeCitySettlementId_))
                {
                    const auto* object =
                        map->objectState().completedObjectAt({x, y});
                    const auto* site =
                        map->objectState().constructionSiteAt({x, y});
                    s << "\nObject: "
                      << (object ? object->objectTypeId : "none")
                      << " | Site: " << (site ? site->objectTypeId : "none");
                    if (site)
                    {
                        s << "\nConstruction progress: "
                          << site->progressPermille / 10.0 << "%";
                    }
                    const auto* owner =
                        world.settlement(activeCitySettlementId_);
                    if (owner)
                    {
                        const auto& citizens =
                            owner->simulationState().citizens();
                        s << "\nWalkable: "
                          << (citizens.navigationDiagnostics()
                                      .walkable(*map, {x, y})
                                  ? "Yes"
                                  : "No");
                        if (const auto* citizen = citizens.citizenAt({x, y}))
                        {
                            s << "\nCitizen: #" << citizen->id.value() << " "
                              << citizen->name
                              << " | Age: " << citizen->ageYears << " | Job: #"
                              << citizen->workplaceId.value();
                        }
                    }
                    if (const auto* road = SettlementObjectCatalog::definition(
                            SettlementObjectTypes::Road
                        ))
                    {
                        s << "\nRoad placeable: "
                          << (map->objectState().canPlace(
                                  map->grid(),
                                  *road,
                                  {{x, y}, 1, 1}
                              )
                                  ? "Yes"
                                  : "No");
                    }
                }
            }
            s << "\nFertility, animals, ground piles, production: not "
                 "implemented";
            cachedStats_ = s.str();
        }
        debugConsole_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
        debugConsole_->render(*renderer_, cachedStats_, *grayUiRenderer_);
    }

    bool Application::handleDebugEvent(const SDL_Event& event)
    {
        const bool wasText = debugConsole_->wantsText();
        const bool consumed = debugConsole_->handle(event);
        if (wasText != debugConsole_->wantsText())
        {
            if (debugConsole_->wantsText())
            {
                SDL_StartTextInput(window_->nativeHandle());
            }
            else
            {
                SDL_StopTextInput(window_->nativeHandle());
            }
        }
        if (consumed)
        {
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_GRAVE)
            {
                if (auto* map =
                        simulation_->settlementMap(activeCitySettlementId_))
                {
                    settlementInspectionPanel_->finishRename(*map, false);
                }
                settlementObjectPlacementController_->cancelPlacement();
                settlementCommandController_->cancel();
            }
            const auto command = debugConsole_->takeCommand();
            if (!command.empty())
            {
                executeConsoleCommand(command);
            }
            return true;
        }

        return false;
    }

} // namespace Paladin
