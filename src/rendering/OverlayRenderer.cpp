#include "rendering/OverlayRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"

#include <algorithm>
#include <array>

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


    void OverlayRenderer::renderOutlines(
        Renderer& renderer,
        std::span<const TileOutlineRenderItem> outlines,
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

        for (const TileOutlineRenderItem& outline : outlines)
        {
            const float x = static_cast<float>(
                viewportWidth * 0.5
                + (outline.tileX - camera.tileX())
                    * tilePixels
            );

            const float y = static_cast<float>(
                viewportHeight * 0.5
                + (outline.tileY - camera.tileY())
                    * tilePixels
            );

            const float width = static_cast<float>(
                outline.widthTiles * tilePixels
            );

            const float height = static_cast<float>(
                outline.heightTiles * tilePixels
            );

            const float lineWidth = std::clamp(
                outline.lineWidthPixels,
                1.0F,
                std::max(1.0F, std::min(width, height) * 0.5F)
            );

            const std::array<RenderRectangle, 4> border{
                RenderRectangle{x, y, width, lineWidth},
                RenderRectangle{
                    x,
                    y + height - lineWidth,
                    width,
                    lineWidth
                },
                RenderRectangle{x, y, lineWidth, height},
                RenderRectangle{
                    x + width - lineWidth,
                    y,
                    lineWidth,
                    height
                }
            };

            renderer.fillRectangles(border, outline.color);
        }
    }
}
