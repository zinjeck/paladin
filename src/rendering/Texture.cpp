#include "rendering/Texture.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    Texture::Texture(SDL_Texture* texture, int width, int height) noexcept
        : texture_(texture), width_(width), height_(height)
    {
    }

    Texture::~Texture()
    {
        if (texture_ && !parent_)
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

    void Texture::setAdditiveBlending(bool enabled) noexcept
    {
        if (!texture_)
        {
            return;
        }
        SDL_SetTextureBlendMode(
            texture_,
            enabled ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND
        );
    }
} // namespace Paladin
