#pragma once

#include "rendering/CityDistantObjects.h"
#include "rendering/CityLighting.h"
#include "rendering/CityPresentation.h"
#include "rendering/GrassPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/SettlementCommandRenderer.h"
#include "rendering/SettlementLogisticsRenderer.h"
#include "rendering/SettlementNaturalFeatureRenderer.h"
#include "rendering/SettlementObjectRenderer.h"
#include "rendering/SettlementStructurePresentation.h"
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
        CityPresentation presentation;
        std::string artRootOverride;
        double animationTimeOverride =
            -1; // Fixed clock for reproducible art previews/tests.
        double animationSeconds = 0;
        std::size_t submittedItems() const
        {
            return raised_.size();
        }
        void reloadArt() const
        {
            raised_.clear();
            sprites_.reset();
            lighting_.reset();
            naturalFeatureRenderer_.invalidateArt();
            objectRenderer_.invalidate();
        }
        void render(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SettlementObjectPlacementController& placementController,
            const SettlementCommandController& commandController,
            const SettlementCitizenState& citizens,
            const SettlementInspectionController& inspection,
            double interpolationAlpha = 1,
            double hour = 12
        ) const;

        void renderMinimap(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const UiRectangle& bounds
        ) const;

    private:
        CityDistantObjects distantObjects_;
        GrassPresentation grass_;
        mutable CityLighting lighting_;
        mutable SceneSpriteLibrary sprites_;
        mutable SceneDrawQueue raised_;
        SettlementStructurePresentation structures_;
        WorldGridRenderer gridRenderer_;
        SettlementLogisticsRenderer logisticsRenderer_;
        SettlementNaturalFeatureRenderer naturalFeatureRenderer_;
        SettlementObjectRenderer objectRenderer_;
        SettlementCommandRenderer commandRenderer_;
        SettlementCitizenRenderer citizenRenderer_;
    };
} // namespace Paladin
