#pragma once

#include <cstdint>
#include <memory>
#include <span>

struct SDL_Renderer;
struct SDL_Surface;

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

    struct RenderRectangle
    {
        float x = 0.0F;
        float y = 0.0F;
        float width = 0.0F;
        float height = 0.0F;
    };

    struct MeshVertex
    {
        float x, y, u, v;
        RenderColor color;
    };

    // Reusable artwork can be composed into a bounded terrain cache without
    // retaining a second CPU copy of every source image.
    struct TextureDrawItem
    {
        const Texture* texture = nullptr;
        RenderRectangle source, destination;
        RenderColor fill{0, 0, 0, 0};
        std::uint8_t opacity = 255;
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
        void compositeLighting(Texture& light, Texture& glow);

        void endFrame();

        void fillRectangle(

            float x,

            float y,

            float width,

            float height,

            RenderColor color

        );

        void drawLine(
            float x1,
            float y1,
            float x2,
            float y2,
            RenderColor color
        );
        void drawMesh(
            const Texture& texture,
            std::span<const MeshVertex> vertices,
            std::span<const int> indices
        );

        void fillRectangles(
            std::span<const RenderRectangle> rectangles,
            RenderColor color
        );

        [[nodiscard]]
        std::unique_ptr<Texture> loadBitmapTexture(const char* filePath);
        std::unique_ptr<Texture> loadImageTexture(
            const char* filePath,
            bool smooth = false
        );

        [[nodiscard]]
        std::unique_ptr<Texture> createTextureFromPixels(
            int width,
            int height,
            std::span<const RenderColor> pixels
        );
        std::unique_ptr<Texture> createEmptyTexture(int width, int height);

        [[nodiscard]]
        std::unique_ptr<Texture> createTextureFromSurface(
            SDL_Surface* surface,
            bool smoothScaling
        );

        std::unique_ptr<Texture> createTextureFromDrawItems(
            int width,
            int height,
            std::span<const TextureDrawItem> items,
            bool premultiplied = false
        );

        [[nodiscard]]
        bool updateTexturePixels(
            Texture& texture,
            std::span<const RenderColor> pixels
        );
        bool updateTextureRegion(
            Texture&,
            int x,
            int y,
            int width,
            int height,
            std::span<const RenderColor>
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
            float destinationHeight,
            std::uint8_t opacity = 255
        );

        [[nodiscard]]

        int outputWidth() const noexcept;

        [[nodiscard]]

        int outputHeight() const noexcept;
        bool beginPixelScene(double pitch);
        void endPixelScene();

    private:
        SDL_Renderer* renderer_ = nullptr;
        std::unique_ptr<Texture> pixelScene_;
        double pixelPitch_ = 1;
    };

} // namespace Paladin
