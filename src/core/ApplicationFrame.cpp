#include "core/Application.h"
#include "core/SimulationClock.h"
#include "rendering/BattleScene.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/CaravanPanel.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/DiplomacyPanel.h"
#include "ui/EmploymentPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldSettlementPanel.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace Paladin
{
    bool Application::simulationControlsVisible() const noexcept
    {
        return screen_ == Screen::Battle || screen_ == Screen::City ||
               (screen_ == Screen::World && simulationControlsUnlocked_);
    }

    void Application::layoutFrame()
    {
        switch (screen_)
        {
        case Screen::MainMenu:
            layoutMainMenu();
            break;
        case Screen::World:
            layoutWorldScreen();
            break;
        case Screen::City:
            layoutCityScreen();
            break;
        case Screen::Battle:
            break;
        }
        if (battleEncounter_ || !battleResultMessage_.empty())
        {
            layoutBattle();
        }
        if (simulationControlsVisible())
        {
            simulationSpeedControls_->layout(renderer_->outputWidth());

            simulationSpeedControls_->setPlaybackState(
                simulationClock_->isPaused(),
                simulationClock_->speedMultiplier()
            );
        }
        debugConsole_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
    }


    bool Application::handleSimulationControlEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            simulationSpeedControls_->pointerMoved(
                event.motion.x,
                event.motion.y
            );
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            simulationControlsCapturedPointer_ =
                simulationSpeedControls_->pointerPressed(
                    event.button.x,
                    event.button.y
                );

            if (simulationControlsCapturedPointer_)
            {
                return true;
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const SimulationSpeedControlAction action =
                simulationSpeedControls_->pointerReleased(
                    event.button.x,
                    event.button.y
                );

            const bool captured = simulationControlsCapturedPointer_;

            simulationControlsCapturedPointer_ = false;

            if (action == SimulationSpeedControlAction::Pause)
            {
                simulationClock_->setPaused(true);
            }
            else if (action == SimulationSpeedControlAction::Normal)
            {
                simulationClock_->setSpeedMultiplier(1.0);
                simulationClock_->setPaused(false);
            }
            else if (action == SimulationSpeedControlAction::Double)
            {
                simulationClock_->setSpeedMultiplier(2.0);
                simulationClock_->setPaused(false);
            }
            else if (action == SimulationSpeedControlAction::Fast)
            {
                const double nextFastSpeed =
                    !simulationClock_->isPaused() &&
                            std::abs(
                                simulationClock_->speedMultiplier() - 3.0
                            ) < 0.001
                        ? 5.0
                        : 3.0;

                simulationClock_->setSpeedMultiplier(nextFastSpeed);
                simulationClock_->setPaused(false);
            }

            if (action != SimulationSpeedControlAction::None)
            {
                simulationSpeedControls_->setPlaybackState(
                    simulationClock_->isPaused(),
                    simulationClock_->speedMultiplier()
                );
            }

            if (captured)
            {
                return true;
            }
        }

        return false;
    }


    void Application::updateFrame()
    {
        updateBattleEncounter();
        switch (screen_)
        {
        case Screen::MainMenu:
            break;
        case Screen::World:
            if (!battleEncounter_ && battleResultMessage_.empty())
            {
                updateWorldScreen();
            }
            break;
        case Screen::City:
            updateCityScreen();
            break;
        case Screen::Battle:
            if (battleScene_)
            {
                battleScene_->update(
                    simulationClock_->frameDeltaSeconds(),
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );
            }
            break;
        }
        const auto simulationDeadline = SDL_GetTicksNS() + 8000000;
        int frameTicks = 0;
        while (simulationClock_->shouldTick() && frameTicks < 4 &&
               (frameTicks == 0 || SDL_GetTicksNS() < simulationDeadline))
        {
            if (screen_ != Screen::MainMenu)
            {
                simulation_->tick(simulationClock_->fixedDeltaSeconds());
            }
            simulationClock_->consumeTick();
            ++frameTicks;
            updateBattleEncounter();
        }
        if (screen_ == Screen::City)
        {
            synchronizeCityStatus();
            updateCityHud();
        }
        if (screen_ != Screen::MainMenu && screen_ != Screen::Battle)
        {
            updateReports();
        }
    }

    void Application::renderFrame()
    {
        renderer_->beginFrame();
        switch (screen_)
        {
        case Screen::MainMenu:
            renderMainMenu();
            break;
        case Screen::World:
            renderWorldScreen();
            break;
        case Screen::City:
            renderCityScreen();
            break;
        case Screen::Battle:
            renderBattleScreen();
            break;
        }
        if (screen_ != Screen::MainMenu && screen_ != Screen::Battle &&
            simulation_)
        {
            ledgerPanel_->render(*renderer_, *grayUiRenderer_);
            renderMilitary();
            renderDebug();
        }
        renderBattleOverlay();
        renderer_->endFrame();
    }

    bool Application::handleEvent(const SDL_Event& event, bool controlsVisible)
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            clearBattle();
            return false;
        }
        if (battleEncounter_ || !battleResultMessage_.empty() ||
            screen_ == Screen::Battle)
        {
            return handleBattleEvent(event);
        }
        if (screen_ != Screen::MainMenu && event.type == SDL_EVENT_KEY_DOWN &&
            !event.key.repeat && event.key.scancode == SDL_SCANCODE_F8)
        {
            SceneSpriteLibrary::setEnvironmentArtEnabled(
                !SceneSpriteLibrary::environmentArtEnabled()
            );
            return true;
        }
        if (screen_ != Screen::MainMenu && event.type == SDL_EVENT_KEY_DOWN &&
            !event.key.repeat && event.key.scancode == SDL_SCANCODE_F6)
        {
            cityRenderer_->reloadArt();
            worldRenderer_->reloadArt();
            cityHud_->reloadArt();
            return true;
        }
        if (screen_ != Screen::MainMenu && simulation_ &&
            handleDebugEvent(event))
        {
            return true;
        }
        if (screen_ == Screen::MainMenu)
        {
            return handleMainMenuEvent(event);
        }
        if (screen_ == Screen::World && controlsVisible &&
            worldSettlementPanel_->handle(event,simulation_->world(),simulation_->playerRealmId()))
        {
            if (worldSettlementPanel_->takeMilitaryRequest())
            {
                handleReportAction(CityHudAction::Military);
            }
            if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) globePointerDown_=globeDragging_=false;
            return true;
        }
        if (screen_ == Screen::World && controlsVisible && diplomacyPanel_->handle(event,simulation_->world(),simulation_->playerRealmId()))
        {
            if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) globePointerDown_=globeDragging_=false;
            return true;
        }
        if (handleCaravanEvent(event))
        {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
                globePointerDown_ = globeDragging_ = false;
            return true;
        }
        if (handleMilitaryEvent(event))
        {
            if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) globePointerDown_=globeDragging_=false;
            return true;
        }
        if (handleReportEvent(event))
        {
            return true;
        }
        // Movable management panels own their pointer capture above the HUD,
        // minimap and speed buttons. A drag must not click through those layers.
        if (screen_ == Screen::World && employmentPanel_->isOpen())
        {
            const bool pointer = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
            const bool inside = pointer ? employmentPanel_->containsPoint(event.button.x,event.button.y) :
                event.type == SDL_EVENT_MOUSE_WHEEL ? employmentPanel_->containsPoint(event.wheel.mouse_x,event.wheel.mouse_y) : false;
            if ((inside || employmentPanel_->capturingPointer() || employmentCapturedPointer_) && handleWorldManagement(event))
                return true;
        }
        if (controlsVisible && handleSimulationControlEvent(event)) return true;
        switch (screen_)
        {
        case Screen::MainMenu:
            break;
        case Screen::World:
            handleWorldEvent(event);
            break;
        case Screen::City:
            handleCityEvent(event);
            break;
        case Screen::Battle:
            break;
        }
        return true;
    }

} // namespace Paladin
