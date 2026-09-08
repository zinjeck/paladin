#include "rendering/Renderer.h"
#include "rendering/Texture.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Paladin
{
    struct PreparedQuadMesh::Data
    {
        std::vector<SDL_Vertex> vertices;
        std::vector<SDL_FPoint> positions;
        std::vector<int> indices;
    };
    PreparedQuadMesh::PreparedQuadMesh(std::span<const MeshVertex> input)
        : data_(std::make_unique<Data>())
    {
        data_->vertices.reserve(input.size());
        data_->positions.reserve(input.size());
        for (const auto& v : input)
        {
            data_->vertices.push_back(
                {{v.x, v.y},
                 {v.color.red / 255.F,
                  v.color.green / 255.F,
                  v.color.blue / 255.F,
                  v.color.alpha / 255.F},
                 {v.u, v.v}}
            );
            data_->positions.push_back({v.x, v.y});
        }
        for (int n = 0; n < int(input.size()); n += 4)
        {
            for (int k : {0, 1, 2, 0, 2, 3})
            {
                data_->indices.push_back(n + k);
            }
        }
    }
    PreparedQuadMesh::~PreparedQuadMesh() = default;
    void Renderer::drawTranslatedQuads(
        const Texture& texture,
        PreparedQuadMesh& mesh,
        float x,
        float y,
        float scale
    )
    {
        auto& data = *mesh.data_;
        auto* out = data.vertices.data();
        const auto* in = data.positions.data();
        const auto count = data.vertices.size();
        for (std::size_t i = 0; i < count; ++i, ++out, ++in)
        {
            out->position = {x + in->x * scale, y + in->y * scale};
        }
        SDL_RenderGeometry(
            renderer_,
            texture.texture_,
            data.vertices.data(),
            int(count),
            data.indices.data(),
            int(data.indices.size())
        );
    }
    void Renderer::drawMesh(
        const Texture& texture,
        std::span<const MeshVertex> vertices,
        std::span<const int> indices
    )
    {
        thread_local std::vector<SDL_Vertex> native;
        native.clear();
        native.reserve(vertices.size());
        for (const auto& v : vertices)
        {
            native.push_back(
                {{v.x, v.y},
                 {v.color.red / 255.F,
                  v.color.green / 255.F,
                  v.color.blue / 255.F,
                  v.color.alpha / 255.F},
                 {v.u, v.v}}
            );
        }
        SDL_RenderGeometry(
            renderer_,
            texture.texture_,
            native.data(),
            int(native.size()),
            indices.data(),
            int(indices.size())
        );
    }
    std::unique_ptr<Texture> Renderer::createEmptyTexture(int width, int height)
    {
        auto* texture = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );
        if (!texture)
        {
            return {};
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        return std::unique_ptr<Texture>(new Texture(texture, width, height));
    }
    Renderer::Renderer(SDL_Window* window)
    {
        renderer_ = SDL_CreateRenderer(window, nullptr);

        if (!renderer_)
        {
            SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());

            return;
        }

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    }

    void Renderer::compositeLighting(Texture& light, Texture& glow)
    {
        SDL_SetTextureScaleMode(light.texture_, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(light.texture_, SDL_BLENDMODE_MOD);
        const SDL_FRect area{0, 0, float(outputWidth()), float(outputHeight())};
        SDL_RenderTexture(renderer_, light.texture_, nullptr, &area);
        SDL_SetTextureScaleMode(glow.texture_, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(glow.texture_, SDL_BLENDMODE_ADD);
        SDL_RenderTexture(renderer_, glow.texture_, nullptr, &area);
        SDL_SetTextureBlendMode(light.texture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(glow.texture_, SDL_BLENDMODE_BLEND);
    }
    Renderer::~Renderer()
    {
        pixelScene_.reset();
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
        SDL_SetRenderDrawColor(renderer_, 18, 20, 24, 255);

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

        const SDL_FRect rectangle{x, y, width, height};

        SDL_RenderFillRect(renderer_, &rectangle);
    }


    void Renderer::drawLine(
        float x1,
        float y1,
        float x2,
        float y2,
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
        SDL_RenderLine(renderer_, x1, y1, x2, y2);
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


    std::unique_ptr<Texture> Renderer::loadImageTexture(
        const char* filePath,
        bool smooth
    )
    {
        auto* surface = IMG_Load(filePath);
        if (!surface)
        {
            SDL_Log(
                "Cannot load sprite %s: %s; retaining placeholder",
                filePath,
                SDL_GetError()
            );
            return nullptr;
        }
        auto result = createTextureFromSurface(surface, smooth);
        SDL_DestroySurface(surface);
        return result;
    }

    std::unique_ptr<Texture> Renderer::loadBitmapTexture(const char* filePath)
    {
        SDL_Surface* surface = SDL_LoadBMP(filePath);

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

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);

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

        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

        return std::unique_ptr<Texture>(new Texture(texture, width, height));
    }


    std::unique_ptr<Texture> Renderer::createTextureFromPixels(
        int width,
        int height,
        std::span<const RenderColor> pixels
    )
    {
        static_assert(sizeof(RenderColor) == 4);

        if (width <= 0 || height <= 0 ||
            pixels.size() != static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height))
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
            SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());

            return nullptr;
        }

        if (!SDL_UpdateTexture(
                texture,
                nullptr,
                pixels.data(),
                width * static_cast<int>(sizeof(RenderColor))
            ))
        {
            SDL_Log("SDL_UpdateTexture failed: %s", SDL_GetError());

            SDL_DestroyTexture(texture);
            return nullptr;
        }

        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        return std::unique_ptr<Texture>(new Texture(texture, width, height));
    }


    std::unique_ptr<Texture> Renderer::createTextureFromSurface(
        SDL_Surface* surface,
        bool smoothScaling
    )
    {
        if (!surface)
        {
            return nullptr;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);

        if (!texture)
        {
            SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
            return nullptr;
        }

        SDL_SetTextureScaleMode(
            texture,
            smoothScaling ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST
        );

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        return std::unique_ptr<Texture>(
            new Texture(texture, surface->w, surface->h)
        );
    }


    std::unique_ptr<Texture> Renderer::createTextureFromDrawItems(
        int width,
        int height,
        std::span<const TextureDrawItem> items,
        bool premultiplied,
        const Texture* batchTexture,
        PreparedQuadMesh* batch
    )
    {
        if (width <= 0 || height <= 0)
        {
            return nullptr;
        }
        SDL_Texture* texture = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            width,
            height
        );
        if (!texture)
        {
            return nullptr;
        }
        SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
        float previousScaleX = 1, previousScaleY = 1;
        SDL_GetRenderScale(renderer_, &previousScaleX, &previousScaleY);
        SDL_Rect previousViewport{}, previousClip{};
        SDL_GetRenderViewport(renderer_, &previousViewport);
        const bool hadViewport = SDL_RenderViewportSet(renderer_);
        const bool hadClip = SDL_RenderClipEnabled(renderer_);
        SDL_GetRenderClipRect(renderer_, &previousClip);
        Uint8 red, green, blue, alpha;
        SDL_GetRenderDrawColor(renderer_, &red, &green, &blue, &alpha);
        if (!SDL_SetRenderTarget(renderer_, texture))
        {
            SDL_DestroyTexture(texture);
            return nullptr;
        }
        SDL_SetRenderViewport(renderer_, nullptr);
        SDL_SetRenderScale(renderer_, 1, 1);
        SDL_SetRenderClipRect(renderer_, nullptr);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
        SDL_RenderClear(renderer_);
        if (batchTexture && batch)
        {
            drawTranslatedQuads(*batchTexture, *batch, 0, 0, 1);
        }
        else
        {
            for (const auto& item : items)
            {
                const auto& d = item.destination;
                if (item.texture)
                {
                    const auto& s = item.source;
                    drawTexture(
                        *item.texture,
                        s.x,
                        s.y,
                        s.width,
                        s.height,
                        d.x,
                        d.y,
                        d.width,
                        d.height,
                        item.opacity
                    );
                }
                else
                {
                    fillRectangle(d.x, d.y, d.width, d.height, item.fill);
                }
            }
        }
        const bool restored = SDL_SetRenderTarget(renderer_, previousTarget);
        SDL_SetRenderScale(renderer_, previousScaleX, previousScaleY);
        SDL_SetRenderViewport(
            renderer_,
            hadViewport ? &previousViewport : nullptr
        );
        SDL_SetRenderClipRect(renderer_, hadClip ? &previousClip : nullptr);
        SDL_SetRenderDrawColor(renderer_, red, green, blue, alpha);
        if (!restored)
        {
            SDL_DestroyTexture(texture);
            return nullptr;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(
            texture,
            premultiplied ? SDL_BLENDMODE_BLEND_PREMULTIPLIED
                          : SDL_BLENDMODE_BLEND
        );
        auto result =
            std::unique_ptr<Texture>(new Texture(texture, width, height));
        result->premultiplied_ = premultiplied;
        return result;
    }


    bool Renderer::updateTextureRegion(
        Texture& texture,
        int x,
        int y,
        int width,
        int height,
        std::span<const RenderColor> pixels
    )
    {
        if (width <= 0 || height <= 0 ||
            pixels.size() != std::size_t(width) * height)
        {
            return false;
        }
        const SDL_Rect rect{x, y, width, height};
        return SDL_UpdateTexture(
            texture.texture_,
            &rect,
            pixels.data(),
            width * int(sizeof(RenderColor))
        );
    }

    bool Renderer::beginPixelScene(double pitch)
    {
        if (pitch <= 1)
        {
            return false;
        }
        const int w = outputWidth(), h = outputHeight();
        if (!pixelScene_ || pixelScene_->width() != w ||
            pixelScene_->height() != h)
        {
            auto* texture = SDL_CreateTexture(
                renderer_,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,
                w,
                h
            );
            if (!texture)
            {
                throw std::runtime_error(
                    "Cannot create mandatory world pixel grid"
                );
            }
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
            pixelScene_.reset(new Texture(texture, w, h));
        }
        if (!SDL_SetRenderTarget(renderer_, pixelScene_->texture_))
        {
            throw std::runtime_error("Cannot activate world pixel grid");
        }
        pixelPitch_ = pitch;
        SDL_SetRenderScale(renderer_, float(1 / pitch), float(1 / pitch));
        SDL_SetRenderDrawColor(renderer_, 18, 20, 24, 255);
        SDL_RenderClear(renderer_);
        return true;
    }
    void Renderer::endPixelScene()
    {
        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_SetRenderScale(renderer_, 1, 1);
        const float w = float(std::ceil(outputWidth() / pixelPitch_));
        const float h = float(std::ceil(outputHeight() / pixelPitch_));
        const SDL_FRect source{0, 0, w, h},
            dest{0, 0, float(w * pixelPitch_), float(h * pixelPitch_)};
        SDL_RenderTexture(renderer_, pixelScene_->texture_, &source, &dest);
        pixelPitch_ = 1;
    }
    bool Renderer::updateTexturePixels(
        Texture& texture,
        std::span<const RenderColor> pixels
    )
    {
        if (pixels.size() != static_cast<std::size_t>(texture.width_) *
                                 static_cast<std::size_t>(texture.height_))
        {
            return false;
        }

        return SDL_UpdateTexture(
            texture.texture_,
            nullptr,
            pixels.data(),
            texture.width_ * static_cast<int>(sizeof(RenderColor))
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
        float destinationHeight,
        std::uint8_t opacity
    )
    {
        if (destinationWidth <= 0 || destinationHeight <= 0 ||
            sourceWidth <= 0 || sourceHeight <= 0)
        {
            return;
        }
        // Crop before handing the scale operation to SDL. Its software path
        // can otherwise scale a whole map into a huge temporary surface and
        // only then clip the result to the window.
        SDL_Rect viewport{};
        SDL_GetRenderViewport(renderer_, &viewport);
        const float left = std::max(0.F, -destinationX),
                    top = std::max(0.F, -destinationY);
        const float right =
            std::min(destinationWidth, float(viewport.w) - destinationX);
        const float bottom =
            std::min(destinationHeight, float(viewport.h) - destinationY);
        if (right <= left || bottom <= top)
        {
            return;
        }
        const SDL_FRect source{
            sourceX + left * sourceWidth / destinationWidth,
            sourceY + top * sourceHeight / destinationHeight,
            (right - left) * sourceWidth / destinationWidth,
            (bottom - top) * sourceHeight / destinationHeight
        };
        const SDL_FRect destination{
            destinationX + left,
            destinationY + top,
            right - left,
            bottom - top
        };

        if (opacity != 255)
        {
            SDL_SetTextureAlphaMod(texture.texture_, opacity);
            if (texture.premultiplied_)
            {
                SDL_SetTextureColorMod(
                    texture.texture_,
                    opacity,
                    opacity,
                    opacity
                );
            }
        }
        SDL_RenderTexture(renderer_, texture.texture_, &source, &destination);
        if (opacity != 255)
        {
            SDL_SetTextureAlphaMod(texture.texture_, 255);
            if (texture.premultiplied_)
            {
                SDL_SetTextureColorMod(texture.texture_, 255, 255, 255);
            }
        }
    }


    int Renderer::outputWidth() const noexcept
    {
        int width = 0;
        int height = 0;

        SDL_GetRenderOutputSize(renderer_, &width, &height);

        return width;
    }


    int Renderer::outputHeight() const noexcept
    {
        int width = 0;
        int height = 0;

        SDL_GetRenderOutputSize(renderer_, &width, &height);

        return height;
    }
} // namespace Paladin
