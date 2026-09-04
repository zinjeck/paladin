#pragma once

#include "rendering/WorldGridRenderer.h"
#include "rendering/SettlementObjectRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SettlementMap;
    class SettlementObjectPlacementController;
    struct TileRenderMetrics;

    class CityRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SettlementObjectPlacementController& placementController
        ) const;

    private:
        WorldGridRenderer gridRenderer_;
        SettlementObjectRenderer objectRenderer_;
    };
}
