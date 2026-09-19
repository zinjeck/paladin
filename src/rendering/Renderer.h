#pragma once

#include "assets/AssetTypes.h"
#include <cstdint>
#include <functional>
#include <string_view>
#include <memory>
#include <optional>
#include <span>
#include <string>

struct SDL_Renderer;
struct SDL_Surface;

struct SDL_Window;

namespace Paladin

{
    class Texture;
    class AssetManager;
    class SceneSpriteLibrary;
    using AssetLoadProgress = std::function<void(std::size_t, std::size_t, std::string_view)>;

    using RenderColor = AssetPixel;
    using RenderRectangle = AssetRectangle;

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

    class PreparedQuadMesh
    {
    public:
        explicit PreparedQuadMesh(std::span<const MeshVertex> vertices);
        ~PreparedQuadMesh();

    private:
        friend class Renderer;
        struct Data;
        std::unique_ptr<Data> data_;
    };

    class Renderer

    {

    public:
        explicit Renderer(SDL_Window* window);
        std::shared_ptr<AssetManager> compiledAssets(const AssetLoadProgress& progress = {});
        // The startup loader and scene facades share atlas views, alpha masks,
        // presentations and recipes. No planet or city is retained here.
        std::shared_ptr<SceneSpriteLibrary> sceneSpriteCache() const { return sceneSpriteCache_; }
        void cacheSceneSprites(std::shared_ptr<SceneSpriteLibrary> value) { sceneSpriteCache_ = std::move(value); }

        ~Renderer();

        Renderer(const Renderer&) = delete;

        Renderer& operator=(const Renderer&) = delete;

        [[nodiscard]]

        bool isValid() const noexcept;

        // Native-output UI clipping. Callers restore the prior rectangle when
        // drawing scrollable graph/table content inside a management window.
        std::optional<RenderRectangle> clipRectangle() const;
        void setClipRectangle(const RenderRectangle* rectangle);
        void beginFrame();
        std::uint64_t frameId() const noexcept { return frameId_; }
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
        void drawTranslatedQuads(
            const Texture&,
            PreparedQuadMesh&,
            float x,
            float y,
            float scale,
            float opacity = 1
        );

        std::shared_ptr<Texture> cacheTextureInAtlas(
            std::shared_ptr<Texture>& page,
            int& x,
            int& y,
            int& row,
            std::unique_ptr<Texture> source
        );
        void drawTextureItems(std::span<const TextureDrawItem> items);

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
        std::shared_ptr<Texture> createTextureView(
            std::shared_ptr<Texture>,
            int x,
            int y,
            int width,
            int height
        );
        void setTextureFiltering(Texture&, bool linear);

        [[nodiscard]]
        std::unique_ptr<Texture> createTextureFromSurface(
            SDL_Surface* surface,
            bool smoothScaling
        );

        std::unique_ptr<Texture> createTextureFromDrawItems(
            int width,
            int height,
            std::span<const TextureDrawItem> items,
            bool premultiplied = false,
            const Texture* batchTexture = nullptr,
            PreparedQuadMesh* batch = nullptr
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
        double currentPixelPitch() const noexcept { return pixelSceneActive_ ? pixelPitch_ : 1.0; }
        bool pixelSceneActive() const noexcept { return pixelSceneActive_; }
        bool usesSoftwareRasterizer() const noexcept;
        bool beginPixelScene(double pitch);
        bool beginPixelScene(double pitch, bool transparent, bool force = false);
        void endPixelScene();
        void endPixelScene(
            std::uint8_t opacity,
            double rotationDegrees,
            double compositeScale,
            double compositeOffsetX = 0.0,
            double compositeOffsetY = 0.0
        );

    private:
        std::uint64_t frameId_ = 0;
        bool activatePixelScene(double pitch);
        bool pixelSceneActive_ = false;
        std::shared_ptr<SceneSpriteLibrary> sceneSpriteCache_;
        std::shared_ptr<AssetManager> assetManager_;
        std::string assetPackageSignature_;
        SDL_Renderer* renderer_ = nullptr;
        std::unique_ptr<Texture> pixelScene_;
        double pixelPitch_ = 1;
        bool pixelSceneTransparent_ = false;
    };

} // namespace Paladin
