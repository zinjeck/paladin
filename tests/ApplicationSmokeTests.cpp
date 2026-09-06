#include "TestFramework.h"
#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace Paladin
{
    // One integration scenario exercises real application routing, not copies
    // of the dispatch logic. Enabled explicitly via CMake for UI refactors.
    struct ApplicationSmokeTest
    {
        static bool send(Application& app, SDL_Event event)
        {
            return app.handleEvent(event, app.simulationControlsVisible());
        }

        static bool click(Application& app, float x, float y)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = x;
            event.button.y = y;
            const bool controlsVisible = app.simulationControlsVisible();
            PALADIN_CHECK(app.handleEvent(event, controlsVisible));
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            return app.handleEvent(event, controlsVisible);
        }

        static void key(Application& app, SDL_Scancode code)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = code;
            PALADIN_CHECK(send(app, event));
        }

        static void frame(Application& app)
        {
            app.simulationClock_->beginFrame();
            app.layoutFrame();
            app.updateFrame();
            app.renderFrame();
        }

        static void run()
        {
            Application app;
            PALADIN_CHECK(app.renderer_ && app.renderer_->isValid());
            const float width = float(app.renderer_->outputWidth());
            const float height = float(app.renderer_->outputHeight());
            frame(app);
            PALADIN_CHECK(click(app, width / 2, height / 2)); // Tutorial.
            PALADIN_CHECK(app.screen_ == Application::Screen::MainMenu);
            PALADIN_CHECK(click(app, width / 2, height / 2 - 60)); // Play.
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            PALADIN_CHECK(!app.simulationControlsVisible());

            // Keep this routing scenario deterministic and small; generation
            // and simulation mechanics have their own existing tests.
            WorldGenerationSettings settings;
            settings.width = settings.height = 64;
            settings.seed = 701;
            app.simulation_ = std::make_unique<Simulation>(settings);
            auto& grid = app.simulation_->world().grid();
            for (int y = 0; y < grid.height(); ++y)
            {
                for (int x = 0; x < grid.width(); ++x)
                {
                    grid.tile({x, y})->terrain = TerrainType::Land;
                    grid.tile({x, y})->elevation = Elevation{.5F};
                }
            }
            app.camera_->setPosition(32, 32);
            frame(app);
            PALADIN_CHECK(click(app, width - 70, height - 22));
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            PALADIN_CHECK(click(app, width / 2, 38)); // Select Region.
            PALADIN_CHECK(app.settlementPlacementController_->isSelecting());
            PALADIN_CHECK(click(app, width / 2, height / 2));
            PALADIN_CHECK(app.foundingPanel_->isOpen());
            const double beforeModalZoom = app.camera_->zoom();
            key(app, SDL_SCANCODE_EQUALS);
            PALADIN_CHECK(app.camera_->zoom() == beforeModalZoom);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(!app.foundingPanel_->isOpen());

            const auto capital = app.simulation_->foundPlayerCapital(
                {32, 32},
                {"Smoke Realm", "Smoke Culture", "Smoke City", {}, "tribal", {}}
            );
            PALADIN_CHECK(capital);
            app.foundingPanel_->openForCapitalRename("Renamed City");
            key(app, SDL_SCANCODE_RETURN);
            PALADIN_CHECK(!app.foundingPanel_->isOpen());
            PALADIN_CHECK(
                app.simulation_->world().settlement(capital)->name() ==
                "Renamed City"
            );
            SettlementMapGenerationSettings local;
            local.localTilesPerWorldTile = 8;
            PALADIN_CHECK(
                app.simulation_->prepareSettlementMap(capital, local)
            );
            frame(app);
            PALADIN_CHECK(click(app, width - 70, height - 22)); // World Play.
            PALADIN_CHECK(app.screen_ == Application::Screen::City);
            PALADIN_CHECK(app.simulationControlsVisible());
            PALADIN_CHECK(
                app.simulation_->detailedSimulationSettlementId() == capital
            );
            frame(app);

            auto& placement = *app.settlementObjectPlacementController_;
            auto* map = app.simulation_->settlementMap(capital);
            PALADIN_CHECK(
                placement.beginPlacement(SettlementObjectTypes::CityKeep)
            );
            placement.pointerMoved(SettlementTilePosition{20, 20});
            const auto original = placement.visibleFootprint();
            PALADIN_CHECK(original);
            key(app, SDL_SCANCODE_F);
            PALADIN_CHECK(
                placement.visibleFootprint()->width == original->height
            );
            PALADIN_CHECK(
                placement.visibleFootprint()->height == original->width
            );

            const float side = SimulationSpeedControls::ButtonSide;
            const auto speedButton = [&](int offset)
            { return click(app, width - side * offset + side / 2, side / 2); };
            PALADIN_CHECK(speedButton(1)); // Pause, without placing a keep.
            PALADIN_CHECK(app.simulationClock_->isPaused());
            PALADIN_CHECK(map->objectState().completedObjects().empty());
            PALADIN_CHECK(!placement.hasLockedFootprint());
            PALADIN_CHECK(speedButton(4));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 1);
            PALADIN_CHECK(!app.simulationClock_->isPaused());
            PALADIN_CHECK(speedButton(3));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 2);
            PALADIN_CHECK(speedButton(2));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 3);
            PALADIN_CHECK(speedButton(2));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 5);
            placement.cancelPlacement();

            app.employmentPanel_->toggle("Employment");
            frame(app);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(!app.employmentPanel_->isOpen());
            PALADIN_CHECK(app.screen_ == Application::Screen::City);
            key(app, SDL_SCANCODE_GRAVE);
            PALADIN_CHECK(app.debugConsole_->wantsText());
            const double beforeConsoleZoom = app.camera_->zoom();
            key(app, SDL_SCANCODE_EQUALS);
            PALADIN_CHECK(app.camera_->zoom() == beforeConsoleZoom);
            key(app, SDL_SCANCODE_GRAVE);
            PALADIN_CHECK(!app.debugConsole_->wantsText());

            const auto beforeTicks = app.simulation_->tickCount();
            app.simulationClock_->reset();
            app.simulationClock_->setPaused(false);
            app.simulationClock_->beginFrame();
            SDL_Delay(70);
            frame(app);
            PALADIN_CHECK(app.simulation_->tickCount() > beforeTicks);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            PALADIN_CHECK(app.simulationControlsVisible());
            PALADIN_CHECK(!app.simulation_->detailedSimulationSettlementId());
            frame(app);
            PALADIN_CHECK(speedButton(1));
            PALADIN_CHECK(app.simulationClock_->isPaused());
            const auto pausedTicks = app.simulation_->tickCount();
            SDL_Delay(70);
            frame(app);
            PALADIN_CHECK(app.simulation_->tickCount() == pausedTicks);
            PALADIN_CHECK(click(app, 70, height - 22)); // World Back.
            PALADIN_CHECK(app.screen_ == Application::Screen::MainMenu);
            PALADIN_CHECK(!app.simulation_);
            frame(app);
            PALADIN_CHECK(!click(app, width / 2, height / 2 + 60)); // Exit.

            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            PALADIN_CHECK(SDL_PushEvent(&quit));
            PALADIN_CHECK(app.run() == 0);
        }
    };
} // namespace Paladin

int main()
{
    try
    {
        Paladin::ApplicationSmokeTest::run();
        std::cout << "Application routing smoke test passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
