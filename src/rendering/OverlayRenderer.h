#pragma once

#include "rendering/Renderer.h"

#include <span>

namespace Paladin
{
    class Camera2D;

    struct TileRenderMetrics;

    struct TileOverlayRenderItem
    {
        double tileX = 0.0;
        double tileY = 0.0;
        double widthTiles = 1.0;
        double heightTiles = 1.0;
        RenderColor color{};
    };

    class OverlayRenderer
    {
    public:
        void render(
            Renderer& renderer,
            std::span<const TileOverlayRenderItem> overlays,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;
    };
}
