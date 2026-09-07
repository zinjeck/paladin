#pragma once
#include "rendering/Renderer.h"
#include <algorithm>
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

    public:
        WorldPixelScene(Renderer& r, double tilePixels)
            : renderer_(r),
              active_(r.beginPixelScene(worldPixelPitch(tilePixels)))
        {
        }
        ~WorldPixelScene()
        {
            if (active_)
            {
                renderer_.endPixelScene();
            }
        }
        WorldPixelScene(const WorldPixelScene&) = delete;
        WorldPixelScene& operator=(const WorldPixelScene&) = delete;
    };
} // namespace Paladin
