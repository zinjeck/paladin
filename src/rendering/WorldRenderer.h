#pragma once

#include "rendering/OverlayRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/WorldGridRenderer.h"

#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    struct TileRenderMetrics;

    class WorldRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            std::span<const SpriteRenderItem> sprites = {},
            std::span<const TileOverlayRenderItem> overlays = {}
        ) const;

    private:
        WorldGridRenderer gridRenderer_;
        SpriteRenderer spriteRenderer_;
        OverlayRenderer overlayRenderer_;
    };
}
