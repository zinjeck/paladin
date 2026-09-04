#include "rendering/SettlementMarkerRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/Settlement.h"

namespace Paladin
{
    void SettlementMarkerRenderer::render(
        Renderer& renderer,
        std::span<const Settlement> settlements,
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

        constexpr float outerMarkerSize = 11.0F;
        constexpr float innerMarkerSize = 7.0F;

        for (const Settlement& settlement : settlements)
        {
            const WorldPosition position =
                settlement.position();

            const float centerX =
                static_cast<float>(
                    viewportWidth * 0.5
                    + (
                        static_cast<double>(position.x) + 0.5
                        - camera.tileX()
                    ) * tilePixels
                );

            const float centerY =
                static_cast<float>(
                    viewportHeight * 0.5
                    + (
                        static_cast<double>(position.y) + 0.5
                        - camera.tileY()
                    ) * tilePixels
                );

            if (
                centerX < -outerMarkerSize ||
                centerY < -outerMarkerSize ||
                centerX > viewportWidth + outerMarkerSize ||
                centerY > viewportHeight + outerMarkerSize
            )
            {
                continue;
            }

            renderer.fillRectangle(
                centerX - outerMarkerSize * 0.5F,
                centerY - outerMarkerSize * 0.5F,
                outerMarkerSize,
                outerMarkerSize,
                {38, 27, 18, 255}
            );

            renderer.fillRectangle(
                centerX - innerMarkerSize * 0.5F,
                centerY - innerMarkerSize * 0.5F,
                innerMarkerSize,
                innerMarkerSize,
                {244, 197, 72, 255}
            );
        }
    }
}
