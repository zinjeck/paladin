#pragma once

struct SDL_Texture;

namespace Paladin
{
    class Renderer;

    class Texture
    {
    public:
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        [[nodiscard]]
        int width() const noexcept;

        [[nodiscard]]
        int height() const noexcept;

    private:
        friend class Renderer;

        Texture(
            SDL_Texture* texture,
            int width,
            int height
        ) noexcept;

        SDL_Texture* texture_ = nullptr;
        int width_ = 0;
        int height_ = 0;
    };
}
