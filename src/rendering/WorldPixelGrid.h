#pragma once
#include "rendering/Renderer.h"
#include <algorithm>
#include <cstdint>
namespace Paladin
{
    // One art pixel is 1/16 of a world tile, including fitted buildings,
    // terrain, entities, attachments and animation. UI is a separate layer.
    inline constexpr int WorldPixelsPerTile = 16;
    inline double worldPixelPitch(double tilePixels)
    {
        return std::max(1.0, tilePixels / WorldPixelsPerTile);
    }
    class WorldPixelScene
    {
        Renderer& renderer_;
        bool active_;
        std::uint8_t opacity_ = 255;
        double rotationDegrees_ = 0.0;
        double compositeScale_ = 1.0;
        double compositeOffsetX_ = 0.0;
        double compositeOffsetY_ = 0.0;

    public:
        WorldPixelScene(
            Renderer& r,
            double tilePixels,
            std::uint8_t opacity = 255,
            double rotationDegrees = 0.0,
            double compositeScale = 1.0,
            double compositeOffsetX = 0.0,
            double compositeOffsetY = 0.0
        )
            : renderer_(r),
              active_(r.beginPixelScene(worldPixelPitch(tilePixels), false,
                  opacity != 255 || rotationDegrees != 0 || compositeScale != 1 ||
                  compositeOffsetX != 0 || compositeOffsetY != 0)),
              opacity_(opacity),
              rotationDegrees_(rotationDegrees),
              compositeScale_(compositeScale),
              compositeOffsetX_(compositeOffsetX),
              compositeOffsetY_(compositeOffsetY)
        {
        }
        ~WorldPixelScene()
        {
            if (active_)
            {
                renderer_.endPixelScene(
                    opacity_,
                    rotationDegrees_,
                    compositeScale_,
                    compositeOffsetX_,
                    compositeOffsetY_
                );
            }
        }
        WorldPixelScene(const WorldPixelScene&) = delete;
        WorldPixelScene& operator=(const WorldPixelScene&) = delete;
    };
} // namespace Paladin
