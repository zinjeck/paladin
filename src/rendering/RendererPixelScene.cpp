#include "rendering/Renderer.h"
#include "rendering/Texture.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace Paladin
{
    bool Renderer::beginPixelScene(double pitch, bool transparent, bool force)
    {
        const bool active = force ? activatePixelScene(std::isfinite(pitch) ? std::max(1.0, pitch) : 1.0) : beginPixelScene(pitch);
        pixelSceneTransparent_ = active && transparent;
        if (pixelSceneTransparent_)
        {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
            SDL_RenderClear(renderer_);
        }
        return active;
    }

    void Renderer::endPixelScene(
        std::uint8_t opacity,
        double rotationDegrees,
        double compositeScale,
        double compositeOffsetX,
        double compositeOffsetY
    )
    {
        if (!pixelSceneTransparent_ && opacity == 255 &&
            std::abs(rotationDegrees) < 1e-9 &&
            std::abs(compositeScale - 1.0) < 1e-9 &&
            std::abs(compositeOffsetX) < 1e-9 &&
            std::abs(compositeOffsetY) < 1e-9)
        {
            endPixelScene();
            return;
        }

        if (!pixelSceneActive_ || !pixelScene_ || !std::isfinite(pixelPitch_) || pixelPitch_ < 1.0)
        {
            pixelPitch_ = 1.0;
            pixelSceneTransparent_ = false;
            return;
        }

        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_SetRenderScale(renderer_, 1.0F, 1.0F);

        const float sourceWidth =
            float(std::ceil(double(outputWidth()) / pixelPitch_));
        const float sourceHeight =
            float(std::ceil(double(outputHeight()) / pixelPitch_));
        const SDL_FRect source{0.0F, 0.0F, sourceWidth, sourceHeight};

        const double safeScale =
            std::isfinite(compositeScale) ? std::max(0.001, compositeScale)
                                          : 1.0;
        const float destinationWidth =
            float(sourceWidth * pixelPitch_ * safeScale);
        const float destinationHeight =
            float(sourceHeight * pixelPitch_ * safeScale);

        SDL_FRect destination{
            0.0F,
            0.0F,
            destinationWidth,
            destinationHeight
        };
        if (std::abs(rotationDegrees) >= 1e-9 ||
            std::abs(safeScale - 1.0) >= 1e-9)
        {
            destination.x =
                float(outputWidth() * (1.0 - safeScale) * 0.5);
            destination.y =
                float(outputHeight() * (1.0 - safeScale) * 0.5);
        }

        // The tangent terrain source camera is snapped only so its INTERNAL
        // nearest-neighbour raster never changes phase. Apply the authoritative
        // camera remainder after rasterization as a rigid physical-screen
        // translation. Callers quantize this offset to whole output pixels, so
        // no terrain texel is independently resampled while panning.
        if (std::isfinite(compositeOffsetX))
        {
            destination.x += float(compositeOffsetX);
        }
        if (std::isfinite(compositeOffsetY))
        {
            destination.y += float(compositeOffsetY);
        }

        // Rotate about the true viewport center, NOT the ceil-padded texture's
        // center. Padding must never shift a chart relative to its annotations.
        const SDL_FPoint pivot{float(outputWidth()*.5*safeScale), float(outputHeight()*.5*safeScale)};
        SDL_SetTextureAlphaMod(pixelScene_->texture_, opacity);
        SDL_SetTextureBlendMode(
            pixelScene_->texture_,
            pixelSceneTransparent_ || opacity < 255 ? SDL_BLENDMODE_BLEND
                                                    : SDL_BLENDMODE_NONE
        );
        SDL_RenderTextureRotated(
            renderer_,
            pixelScene_->texture_,
            &source,
            &destination,
            std::isfinite(rotationDegrees) ? rotationDegrees : 0.0,
            &pivot,
            SDL_FLIP_NONE
        );
        SDL_SetTextureAlphaMod(pixelScene_->texture_, 255);
        SDL_SetTextureBlendMode(pixelScene_->texture_, SDL_BLENDMODE_NONE);
        pixelPitch_ = 1.0;
        pixelSceneTransparent_ = false;
        pixelSceneActive_ = false;
    }
} // namespace Paladin
