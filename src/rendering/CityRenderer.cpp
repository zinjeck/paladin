#include "rendering/CityRenderer.h"
#include "ui/UiTypes.h"

#include <algorithm>

#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementCommandController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

namespace Paladin
{
    void CityRenderer::renderMinimap(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const UiRectangle& bounds
    ) const
    {
        constexpr float border = 3.0F;
        if (bounds.width <= border * 2 || bounds.height <= border * 2)
        {
            return;
        }
        const auto& grid = settlementMap.grid();
        const float scale = std::min(
            (bounds.width - border * 2) / grid.width(),
            (bounds.height - border * 2) / grid.height()
        );
        const float width = grid.width() * scale;
        const float height = grid.height() * scale;
        const float x = bounds.x + (bounds.width - width) * 0.5F;
        const float y = bounds.y + (bounds.height - height) * 0.5F;
        gridRenderer_.renderOverview(renderer, x, y, width, height);

        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        if (tilePixels <= 0.0)
        {
            return;
        }
        const double halfWidth = renderer.outputWidth() / tilePixels * 0.5;
        const double halfHeight = renderer.outputHeight() / tilePixels * 0.5;
        const float left = x + static_cast<float>(std::clamp(
            camera.tileX() - halfWidth, 0.0, double(grid.width()))) * scale;
        const float right = x + static_cast<float>(std::clamp(
            camera.tileX() + halfWidth, 0.0, double(grid.width()))) * scale;
        const float top = y + static_cast<float>(std::clamp(
            camera.tileY() - halfHeight, 0.0, double(grid.height()))) * scale;
        const float bottom = y + static_cast<float>(std::clamp(
            camera.tileY() + halfHeight, 0.0, double(grid.height()))) * scale;
        if (right <= left || bottom <= top)
        {
            return;
        }
        const RenderColor outline{255, 255, 255, 255};
        const float stroke = std::min({1.5F, right - left, bottom - top});
        renderer.fillRectangle(left, top, right - left, stroke, outline);
        renderer.fillRectangle(left, bottom - stroke, right - left, stroke, outline);
        renderer.fillRectangle(left, top, stroke, bottom - top, outline);
        renderer.fillRectangle(right - stroke, top, stroke, bottom - top, outline);
    }


    void CityRenderer::render(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SettlementObjectPlacementController& placementController,
        const SettlementCommandController& commandController,
        const SettlementCitizenState& citizens
    ) const
    {
        gridRenderer_.render(
            renderer,
            settlementMap.grid(),
            camera,
            metrics
        );

        naturalFeatureRenderer_.render(renderer, settlementMap, camera, metrics);

        objectRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController
        );

        commandRenderer_.render(
            renderer,
            settlementMap.commandState(),
            commandController,
            camera,
            metrics
        );

        citizenRenderer_.render(renderer, citizens, camera, metrics);
    }
}
