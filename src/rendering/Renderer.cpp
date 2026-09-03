#include "rendering/Renderer.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    Renderer::Renderer(SDL_Window* window)
    {
        renderer_ = SDL_CreateRenderer(window, nullptr);

        if (!renderer_)
        {
            SDL_Log(
                "SDL_CreateRenderer failed: %s",
                SDL_GetError()
            );
        }
    }

    Renderer::~Renderer()
    {
        if (renderer_)
        {
            SDL_DestroyRenderer(renderer_);
        }
    }

    bool Renderer::isValid() const noexcept
    {
        return renderer_ != nullptr;
    }

    void Renderer::beginFrame()
    {
        SDL_SetRenderDrawColor(
            renderer_,
            18,
            20,
            24,
            255
        );

        SDL_RenderClear(renderer_);
    }

    void Renderer::endFrame()
    {
        SDL_RenderPresent(renderer_);
    }

    void Renderer::fillRectangle(
        float x,
        float y,
        float width,
        float height,
        RenderColor color
    )
    {
        SDL_SetRenderDrawColor(
            renderer_,
            color.red,
            color.green,
            color.blue,
            color.alpha
        );
    
        const SDL_FRect rectangle{
            x,
            y,
            width,
            height
        };
    
        SDL_RenderFillRect(
            renderer_,
            &rectangle
        );
    }
    
    
    int Renderer::outputWidth() const noexcept
    {
        int width = 0;
        int height = 0;
    
        SDL_GetRenderOutputSize(
            renderer_,
            &width,
            &height
        );
    
        return width;
    }
    
    
    int Renderer::outputHeight() const noexcept
    {
        int width = 0;
        int height = 0;
    
        SDL_GetRenderOutputSize(
            renderer_,
            &width,
            &height
        );
    
        return height;
    }
}