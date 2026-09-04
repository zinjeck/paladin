#include "rendering/WorldRenderer.h"

#include "world/World.h"

namespace Paladin
{
    void WorldRenderer::render(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        std::span<const SpriteRenderItem> sprites,
        std::span<const TileOverlayRenderItem> overlays
    ) const
    {
        gridRenderer_.render(
            renderer,
            world.grid(),
            camera,
            metrics
        );

        spriteRenderer_.render(
            renderer,
            sprites,
            camera,
            metrics
        );

        overlayRenderer_.render(
            renderer,
            overlays,
            camera,
            metrics
        );
    }
}
