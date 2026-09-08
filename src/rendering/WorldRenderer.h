#pragma once

#include "rendering/GlobeRenderer.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementMarkerRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/TerritoryPresentationPolicy.h"
#include "rendering/TerritoryRenderer.h"
#include "rendering/WorldCartography.h"
#include "rendering/WorldGridRenderer.h"

#include "rendering/WorldMapNavigation.h"
#include "ui/GrayUiRenderer.h"
#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    struct TileRenderMetrics;

    class WorldRenderer
    {
    public:
        double animationSeconds = 0;
        bool globeEnabled = false;
        bool politicalViewRequested = false;
        WorldRenderer();

        explicit WorldRenderer(
            TerritoryPresentationPolicy territoryPresentationPolicy
        );

        std::uint64_t terrainAtlasBuilds() const
        {
            return globe_.atlasBuilds;
        }
        bool terrainDetailReady() const
        {
            return globe_.detailReady();
        }
        bool terrainLocalDetailReady() const { return globe_.fullDetailReady(); }
        void reloadArt() const
        {
            artwork_.reset();
            cartography_.reset();
            globe_.reset();
        }

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            std::span<const SpriteRenderItem> sprites = {},
            std::span<const TileOverlayRenderItem> overlays = {},
            std::span<const TileOutlineRenderItem> outlines = {}
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
        std::optional<PlanetRotation> lastGlobeRotation_;
        WorldGridRenderer gridRenderer_;
        mutable WorldCartography cartography_;
        mutable SceneSpriteLibrary artwork_;
        mutable GlobeRenderer globe_;
        TerritoryRenderer territoryRenderer_;
        SpriteRenderer spriteRenderer_;
        SettlementMarkerRenderer settlementMarkerRenderer_;
        OverlayRenderer overlayRenderer_;
        TerritoryPresentationPolicy territoryPresentationPolicy_;
        mutable bool politicalViewActive_ = false;
        mutable bool politicalViewInitialized_ = false;
    };
} // namespace Paladin
