#include "rendering/Renderer.h"
#include "rendering/Texture.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace Paladin
{
    void Renderer::endPixelScene(
        std::uint8_t opacity,
        double rotationDegrees,
        double compositeScale
    )
    {
        if (opacity == 255 && std::abs(rotationDegrees) < 1e-9 &&
            std::abs(compositeScale - 1.0) < 1e-9)
        {
            endPixelScene();
            return;
        }

        if (!pixelScene_ || !std::isfinite(pixelPitch_) || pixelPitch_ <= 1.0)
        {
            pixelPitch_ = 1.0;
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
                (float(outputWidth()) - destinationWidth) * 0.5F;
            destination.y =
                (float(outputHeight()) - destinationHeight) * 0.5F;
        }

        SDL_SetTextureAlphaMod(pixelScene_->texture_, opacity);
        SDL_SetTextureBlendMode(
            pixelScene_->texture_,
            opacity < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE
        );
        SDL_RenderTextureRotated(
            renderer_,
            pixelScene_->texture_,
            &source,
            &destination,
            std::isfinite(rotationDegrees) ? rotationDegrees : 0.0,
            nullptr,
            SDL_FLIP_NONE
        );
        SDL_SetTextureAlphaMod(pixelScene_->texture_, 255);
        SDL_SetTextureBlendMode(pixelScene_->texture_, SDL_BLENDMODE_NONE);
        pixelPitch_ = 1.0;
    }
} // namespace Paladin
