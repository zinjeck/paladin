#pragma once

#include <cstdint>
#include <memory>

struct SDL_Renderer;

struct SDL_Window;

namespace Paladin

{
    class Texture;

    struct RenderColor

    {

        std::uint8_t red = 255;

        std::uint8_t green = 255;

        std::uint8_t blue = 255;

        std::uint8_t alpha = 255;

    };

    class Renderer

    {

    public:

        explicit Renderer(SDL_Window* window);

        ~Renderer();

        Renderer(const Renderer&) = delete;

        Renderer& operator=(const Renderer&) = delete;

        [[nodiscard]]

        bool isValid() const noexcept;

        void beginFrame();

        void endFrame();

        void fillRectangle(

            float x,

            float y,

            float width,

            float height,

            RenderColor color

        );

        [[nodiscard]]
        std::unique_ptr<Texture> loadBitmapTexture(
            const char* filePath
        );

        void drawTexture(
            const Texture& texture,
            float sourceX,
            float sourceY,
            float sourceWidth,
            float sourceHeight,
            float destinationX,
            float destinationY,
            float destinationWidth,
            float destinationHeight
        );

        [[nodiscard]]

        int outputWidth() const noexcept;

        [[nodiscard]]

        int outputHeight() const noexcept;

    private:

        SDL_Renderer* renderer_ = nullptr;

    };

}
