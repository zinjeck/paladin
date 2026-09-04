#include "rendering/SpriteRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"

namespace Paladin
{
    void SpriteRenderer::render(
        Renderer& renderer,
        std::span<const SpriteRenderItem> sprites,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        const double tilePixels =
            metrics.scaledTilePixels(camera.zoom());

        const float viewportWidth =
            static_cast<float>(renderer.outputWidth());

        const float viewportHeight =
            static_cast<float>(renderer.outputHeight());

        for (const SpriteRenderItem& sprite : sprites)
        {
            if (!sprite.texture)
            {
                continue;
            }

            const float sourceWidth =
                sprite.sourceWidth > 0.0F
                    ? sprite.sourceWidth
                    : static_cast<float>(sprite.texture->width());

            const float sourceHeight =
                sprite.sourceHeight > 0.0F
                    ? sprite.sourceHeight
                    : static_cast<float>(sprite.texture->height());

            const float width =
                (sprite.displayWidthPixels > 0.0F
                    ? sprite.displayWidthPixels
                    : sourceWidth)
                * static_cast<float>(camera.zoom());

            const float height =
                (sprite.displayHeightPixels > 0.0F
                    ? sprite.displayHeightPixels
                    : sourceHeight)
                * static_cast<float>(camera.zoom());

            const float anchorScreenX =
                viewportWidth * 0.5F
                + static_cast<float>(
                    (sprite.tileX - camera.tileX())
                    * tilePixels
                );

            const float anchorScreenY =
                viewportHeight * 0.5F
                + static_cast<float>(
                    (sprite.tileY - camera.tileY())
                    * tilePixels
                );

            const float destinationX =
                anchorScreenX - width * sprite.anchorX;

            const float destinationY =
                anchorScreenY - height * sprite.anchorY;

            if (
                destinationX + width < 0.0F ||
                destinationY + height < 0.0F ||
                destinationX > viewportWidth ||
                destinationY > viewportHeight
            )
            {
                continue;
            }

            renderer.drawTexture(
                *sprite.texture,
                sprite.sourceX,
                sprite.sourceY,
                sourceWidth,
                sourceHeight,
                destinationX,
                destinationY,
                width,
                height
            );
        }
    }
}
