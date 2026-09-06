#include "core/Application.h"
#include "rendering/Renderer.h"
#include "ui/MainMenu.h"
#include <SDL3/SDL.h>

namespace Paladin
{
    void Application::layoutMainMenu()
    {
        mainMenu_->layout(renderer_->outputWidth(), renderer_->outputHeight());
    }

    bool Application::handleMainMenuEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            mainMenu_->pointerMoved(event.motion.x, event.motion.y);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            mainMenu_->pointerPressed(event.button.x, event.button.y);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const MainMenuAction action =
                mainMenu_->pointerReleased(event.button.x, event.button.y);

            if (action == MainMenuAction::Play)
            {
                startWorldSession();
            }
            else if (action == MainMenuAction::Exit)
            {
                return false;
            }

            // Tutorial intentionally has no action yet.
        }

        return true;
    }

    void Application::renderMainMenu()
    {
        mainMenu_->render(*renderer_, *grayUiRenderer_);
    }

} // namespace Paladin
