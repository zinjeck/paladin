#pragma once

#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/SettlementCommandRenderer.h"
#include "rendering/SettlementLogisticsRenderer.h"
#include "rendering/SettlementNaturalFeatureRenderer.h"
#include "rendering/SettlementObjectRenderer.h"
#include "rendering/WorldGridRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SettlementInspectionController;
    class SettlementMap;
    class SettlementObjectPlacementController;
    class SettlementCommandController;
    class SettlementCitizenState;
    struct TileRenderMetrics;
    struct UiRectangle;

    class CityRenderer
    {
    public:
      void render(
          Renderer& renderer,
          const SettlementMap& settlementMap,
          const Camera2D& camera,
          const TileRenderMetrics& metrics,
          const SettlementObjectPlacementController& placementController,
          const SettlementCommandController& commandController,
          const SettlementCitizenState& citizens,
          const SettlementInspectionController& inspection
      ) const;

      void renderMinimap(
          Renderer& renderer,
          const SettlementMap& settlementMap,
          const Camera2D& camera,
          const TileRenderMetrics& metrics,
          const UiRectangle& bounds
      ) const;

    private:
        WorldGridRenderer gridRenderer_;
        SettlementLogisticsRenderer logisticsRenderer_;
        SettlementNaturalFeatureRenderer naturalFeatureRenderer_;
        SettlementObjectRenderer objectRenderer_;
        SettlementCommandRenderer commandRenderer_;
        SettlementCitizenRenderer citizenRenderer_;
    };
}
