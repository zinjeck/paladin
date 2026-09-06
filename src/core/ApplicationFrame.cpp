#include "core/Application.h"
#include "core/SimulationClock.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/DebugConsole.h"
#include "ui/SimulationSpeedControls.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace Paladin
{
    bool Application::simulationControlsVisible() const noexcept
    {
        return screen_ == Screen::City ||
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
        switch (screen_)
        {
        case Screen::MainMenu:
            break;
        case Screen::World:
            updateWorldScreen();
            break;
        case Screen::City:
            updateCityScreen();
            break;
        }
        while (simulationClock_->shouldTick())
        {
            if (screen_ != Screen::MainMenu)
            {
                simulation_->tick(simulationClock_->fixedDeltaSeconds());
            }
            simulationClock_->consumeTick();
        }
        if (screen_ == Screen::City)
        {
            synchronizeCityStatus();
            updateCityHud();
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
        }
        if (screen_ != Screen::MainMenu && simulation_)
        {
            renderDebug();
        }
        renderer_->endFrame();
    }

    bool Application::handleEvent(const SDL_Event& event, bool controlsVisible)
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            return false;
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
        if (controlsVisible && handleSimulationControlEvent(event))
        {
            return true;
        }
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
        }
        return true;
    }

} // namespace Paladin
