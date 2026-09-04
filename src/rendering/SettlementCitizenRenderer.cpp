#include "rendering/SettlementCitizenRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include <vector>

namespace Paladin
{
    void SettlementCitizenRenderer::render(
        Renderer& renderer,
        const SettlementCitizenState& citizens,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        const float markerSize = static_cast<float>(tilePixels * 0.5);
        std::vector<RenderRectangle> rectangles;
        rectangles.reserve(citizens.citizens().size());

        for (const SettlementCitizen& citizen : citizens.citizens())
        {
            const double centerX =
                static_cast<double>(renderer.outputWidth()) * 0.5 +
                (static_cast<double>(citizen.tilePosition.x) + 0.5 -
                    camera.tileX()) * tilePixels;
            const double centerY =
                static_cast<double>(renderer.outputHeight()) * 0.5 +
                (static_cast<double>(citizen.tilePosition.y) + 0.5 -
                    camera.tileY()) * tilePixels;

            if (
                centerX + markerSize < 0.0 || centerY + markerSize < 0.0 ||
                centerX - markerSize > renderer.outputWidth() ||
                centerY - markerSize > renderer.outputHeight()
            )
            {
                continue;
            }

            rectangles.push_back({
                static_cast<float>(centerX) - markerSize * 0.5F,
                static_cast<float>(centerY) - markerSize * 0.5F,
                markerSize,
                markerSize
            });
        }

        renderer.fillRectangles(rectangles, {210, 180, 140, 255});
    }
}
