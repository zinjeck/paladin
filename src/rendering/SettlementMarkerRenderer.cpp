#include "rendering/SettlementMarkerRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/Settlement.h"
#include "world/World.h"

#include <algorithm>

namespace Paladin
{
    void SettlementMarkerRenderer::render(
        Renderer& renderer,
        const World& world,
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

        for (const Settlement& settlement : world.settlements())
        {
            const WorldTilePosition position =
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

            RenderColor markerColor{244, 197, 72, 255};

            if (const Polity* polity =
                    world.polity(settlement.ownerPolityId()))
            {
                const MapColor mapColor = polity->mapColor();
                markerColor = {
                    mapColor.red,
                    mapColor.green,
                    mapColor.blue,
                    255
                };
            }

            renderer.fillRectangle(
                centerX - innerMarkerSize * 0.5F,
                centerY - innerMarkerSize * 0.5F,
                innerMarkerSize,
                innerMarkerSize,
                markerColor
            );

            if (settlement.name().empty())
            {
                continue;
            }

            constexpr float preferredPixelSize = 2.0F;
            constexpr float maximumLabelWidth = 180.0F;

            const float preferredLabelWidth =
                fontRenderer_.measureWidth(
                    settlement.name(),
                    preferredPixelSize
                );

            const float pixelSize =
                preferredLabelWidth > maximumLabelWidth
                    ? preferredPixelSize
                        * maximumLabelWidth
                        / preferredLabelWidth
                    : preferredPixelSize;

            const float labelWidth =
                fontRenderer_.measureWidth(
                    settlement.name(),
                    pixelSize
                );

            const float labelX = centerX - labelWidth * 0.5F;
            const float labelY =
                centerY
                - outerMarkerSize * 0.5F
                - 7.0F * pixelSize
                - 5.0F;

            fontRenderer_.drawText(
                renderer,
                settlement.name(),
                labelX + 1.0F,
                labelY + 1.0F,
                pixelSize,
                {0, 0, 0, 220}
            );

            fontRenderer_.drawText(
                renderer,
                settlement.name(),
                labelX,
                labelY,
                pixelSize,
                {246, 246, 248, 255}
            );
        }
    }
}
