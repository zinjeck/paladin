#include "rendering/CityRenderer.h"

#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementCommandController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

namespace Paladin
{
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
