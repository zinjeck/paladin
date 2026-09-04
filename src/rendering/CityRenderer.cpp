#include "rendering/CityRenderer.h"

#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"

namespace Paladin
{
    void CityRenderer::render(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SettlementObjectPlacementController& placementController
    ) const
    {
        gridRenderer_.render(
            renderer,
            settlementMap.grid(),
            camera,
            metrics
        );

        objectRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController
        );
    }
}
