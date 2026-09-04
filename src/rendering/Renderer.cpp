#include "rendering/Renderer.h"
#include "rendering/Texture.h"

#include <SDL3/SDL.h>

#include <cstddef>

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

            return;
        }

        SDL_SetRenderDrawBlendMode(
            renderer_,
            SDL_BLENDMODE_BLEND
        );
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


    void Renderer::fillRectangles(
        std::span<const RenderRectangle> rectangles,
        RenderColor color
    )
    {
        static_assert(sizeof(RenderRectangle) == sizeof(SDL_FRect));
        static_assert(alignof(RenderRectangle) == alignof(SDL_FRect));

        if (rectangles.empty())
        {
            return;
        }

        SDL_SetRenderDrawColor(
            renderer_,
            color.red,
            color.green,
            color.blue,
            color.alpha
        );

        SDL_RenderFillRects(
            renderer_,
            reinterpret_cast<const SDL_FRect*>(rectangles.data()),
            static_cast<int>(rectangles.size())
        );
    }


    std::unique_ptr<Texture> Renderer::loadBitmapTexture(
        const char* filePath
    )
    {
        SDL_Surface* surface =
            SDL_LoadBMP(filePath);

        if (!surface)
        {
            SDL_Log(
                "SDL_LoadBMP failed for '%s': %s",
                filePath,
                SDL_GetError()
            );

            return nullptr;
        }

        const int width = surface->w;
        const int height = surface->h;

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(
                renderer_,
                surface
            );

        SDL_DestroySurface(surface);

        if (!texture)
        {
            SDL_Log(
                "SDL_CreateTextureFromSurface failed for '%s': %s",
                filePath,
                SDL_GetError()
            );

            return nullptr;
        }

        SDL_SetTextureScaleMode(
            texture,
            SDL_SCALEMODE_NEAREST
        );

        return std::unique_ptr<Texture>(
            new Texture(
                texture,
                width,
                height
            )
        );
    }


    std::unique_ptr<Texture> Renderer::createTextureFromPixels(
        int width,
        int height,
        std::span<const RenderColor> pixels
    )
    {
        static_assert(sizeof(RenderColor) == 4);

        if (
            width <= 0 ||
            height <= 0 ||
            pixels.size() !=
                static_cast<std::size_t>(width)
                    * static_cast<std::size_t>(height)
        )
        {
            return nullptr;
        }

        SDL_Texture* texture = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            width,
            height
        );

        if (!texture)
        {
            SDL_Log(
                "SDL_CreateTexture failed: %s",
                SDL_GetError()
            );

            return nullptr;
        }

        if (!SDL_UpdateTexture(
                texture,
                nullptr,
                pixels.data(),
                width * static_cast<int>(sizeof(RenderColor))
            ))
        {
            SDL_Log(
                "SDL_UpdateTexture failed: %s",
                SDL_GetError()
            );

            SDL_DestroyTexture(texture);
            return nullptr;
        }

        SDL_SetTextureScaleMode(
            texture,
            SDL_SCALEMODE_NEAREST
        );

        SDL_SetTextureBlendMode(
            texture,
            SDL_BLENDMODE_BLEND
        );

        return std::unique_ptr<Texture>(
            new Texture(texture, width, height)
        );
    }


    bool Renderer::updateTexturePixels(
        Texture& texture,
        std::span<const RenderColor> pixels
    )
    {
        if (
            pixels.size() !=
                static_cast<std::size_t>(texture.width_)
                    * static_cast<std::size_t>(texture.height_)
        )
        {
            return false;
        }

        return SDL_UpdateTexture(
            texture.texture_,
            nullptr,
            pixels.data(),
            texture.width_
                * static_cast<int>(sizeof(RenderColor))
        );
    }


    void Renderer::drawTexture(
        const Texture& texture,
        float sourceX,
        float sourceY,
        float sourceWidth,
        float sourceHeight,
        float destinationX,
        float destinationY,
        float destinationWidth,
        float destinationHeight
    )
    {
        const SDL_FRect source{
            sourceX,
            sourceY,
            sourceWidth,
            sourceHeight
        };

        const SDL_FRect destination{
            destinationX,
            destinationY,
            destinationWidth,
            destinationHeight
        };

        SDL_RenderTexture(
            renderer_,
            texture.texture_,
            &source,
            &destination
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
