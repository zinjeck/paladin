#pragma once

#include "rendering/GlobeRenderer.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementMarkerRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/WorldGridRenderer.h"
#include "rendering/WorldPixelStability.h"
#include "rendering/WorldPresentation.h"
#include "rendering/WorldTerritoryPresentationRenderer.h"

#include "rendering/WorldMapNavigation.h"
#include "ui/GrayUiRenderer.h"
#include "world/WorldTilePosition.h"
#include <cstdint>
#include <optional>
#include <span>

namespace Paladin
{
    class Camera2D;
    class World;

    struct TileRenderMetrics;

    struct WorldPlacementMarker
    {
        WorldTilePosition position;
        RenderColor color{255, 215, 131, 235};
    };

    class WorldRenderer
    {
    public:
        double animationSeconds = 0;
        bool globeEnabled = false;
        WorldRenderer();

        explicit WorldRenderer(WorldPresentationPolicy worldPresentationPolicy);

        std::uint64_t terrainAtlasBuilds() const
        {
            return globe_.atlasBuilds;
        }
        bool terrainDetailReady() const
        {
            return globe_.detailReady();
        }
        bool terrainLocalDetailReady() const
        {
            return globe_.fullDetailReady();
        }
        bool prepareTerrain(Renderer&, const World&) const;
        float terrainPreparationProgress() const
        {
            return globe_.preparationProgress();
        }
        void reloadArt() const
        {
            artwork_.reset();
            globe_.reset();
            territoryPresentationRenderer_.reset();
            pixelStabilityActive_ = false;
        }

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            std::span<const SpriteRenderItem> sprites = {},
            std::span<const TileOverlayRenderItem> overlays = {},
            std::span<const TileOutlineRenderItem> outlines = {},
            std::optional<WorldPlacementMarker> placementMarker = std::nullopt
        ) const;

        void renderNavigator(
            Renderer&,
            const World&,
            const Camera2D&,
            const TileRenderMetrics&,
            const GrayUiRenderer&
        ) const;
        void toggleProjection(
            Camera2D&,
            const WorldGrid&,
            int,
            int,
            const TileRenderMetrics&
        );

    private:
        [[nodiscard]]
        double effectiveTilePixels(
            const Renderer&,
            const World&,
            const Camera2D&,
            const TileRenderMetrics&
        ) const noexcept;

        void drawSettlementPlacementMarker(
            Renderer&,
            const World&,
            const Camera2D&,
            double effectiveTilePixels,
            const WorldPlacementMarker&
        ) const;

        std::optional<PlanetRotation> lastGlobeRotation_;
        WorldGridRenderer gridRenderer_;
        mutable SceneSpriteLibrary artwork_;
        mutable GlobeRenderer globe_;
        SpriteRenderer spriteRenderer_;
        SettlementMarkerRenderer settlementMarkerRenderer_;
        OverlayRenderer overlayRenderer_;
        mutable WorldTerritoryPresentationRenderer territoryPresentationRenderer_;
        WorldPresentationPolicy worldPresentationPolicy_;
        WorldPixelStabilityPolicy pixelStabilityPolicy_;
        mutable bool pixelStabilityActive_ = false;
    };
} // namespace Paladin
