#include "rendering/OverlayRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"

namespace Paladin
{
    void OverlayRenderer::render(
        Renderer& renderer,
        std::span<const TileOverlayRenderItem> overlays,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        const double tilePixels =
            metrics.scaledTilePixels(camera.zoom());

        const double viewportWidth =
            static_cast<double>(renderer.outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer.outputHeight());

        for (const TileOverlayRenderItem& overlay : overlays)
        {
            renderer.fillRectangle(
                static_cast<float>(
                    viewportWidth * 0.5
                    + (overlay.tileX - camera.tileX())
                        * tilePixels
                ),
                static_cast<float>(
                    viewportHeight * 0.5
                    + (overlay.tileY - camera.tileY())
                        * tilePixels
                ),
                static_cast<float>(
                    overlay.widthTiles * tilePixels
                ),
                static_cast<float>(
                    overlay.heightTiles * tilePixels
                ),
                overlay.color
            );
        }
    }
}
