#pragma once

#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class Texture;

    struct TileRenderMetrics;

    struct SpriteRenderItem
    {
        const Texture* texture = nullptr;

        // The sprite is anchored in logical world-tile space.
        double tileX = 0.0;
        double tileY = 0.0;

        // A non-positive source size selects the full texture.
        float sourceX = 0.0F;
        float sourceY = 0.0F;
        float sourceWidth = 0.0F;
        float sourceHeight = 0.0F;

        // Display dimensions are pixels at zoom 1, independent of tile size.
        // A non-positive value uses the selected source dimension.
        float displayWidthPixels = 0.0F;
        float displayHeightPixels = 0.0F;

        // Normalized anchor within the displayed sprite.
        float anchorX = 0.5F;
        float anchorY = 1.0F;
    };

    class SpriteRenderer
    {
    public:
        void render(
            Renderer& renderer,
            std::span<const SpriteRenderItem> sprites,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;
    };
}
