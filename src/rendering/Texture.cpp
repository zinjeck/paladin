#include "rendering/Texture.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    Texture::Texture(
        SDL_Texture* texture,
        int width,
        int height
    ) noexcept
        : texture_(texture),
          width_(width),
          height_(height)
    {
    }

    Texture::~Texture()
    {
        if (texture_)
        {
            SDL_DestroyTexture(texture_);
        }
    }

    int Texture::width() const noexcept
    {
        return width_;
    }

    int Texture::height() const noexcept
    {
        return height_;
    }
}
