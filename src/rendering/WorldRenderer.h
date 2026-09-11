#pragma once

#include "rendering/CelestialSun.h"
#include "rendering/GlobeRenderer.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/WorldCartography.h"
#include "rendering/WorldGridRenderer.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldPixelStability.h"
#include "rendering/WorldPresentation.h"
#include "rendering/WorldRealmPresentationRenderer.h"

#include "rendering/WorldMapNavigation.h"
#include "ui/GrayUiRenderer.h"
#include <cstdint>
#include <optional>
#include <span>

namespace Paladin
{
    class Camera2D;
    class World;

    struct TileRenderMetrics;

    class WorldRenderer
    {
    public:
        double animationSeconds = 0;
        bool globeEnabled = false;
        WorldRenderer();

        explicit WorldRenderer(WorldPresentationPolicy worldPresentationPolicy);

        [[nodiscard]]
        WorldMapMode mapMode() const noexcept
        {
            return mapMode_;
        }

        void setMapMode(WorldMapMode mode) noexcept
        {
            mapMode_ = mode;
        }

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
            sunRenderer_.reset();
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

        std::optional<PlanetRotation> lastGlobeRotation_;
        WorldGridRenderer gridRenderer_;
        mutable SceneSpriteLibrary artwork_;
        mutable GlobeRenderer globe_;
        mutable CelestialSunRenderer sunRenderer_;
        OverlayRenderer overlayRenderer_;
        mutable WorldRealmPresentationRenderer territoryPresentationRenderer_;
        WorldObjectRenderer worldObjectRenderer_;
        WorldPresentationPolicy worldPresentationPolicy_;
        WorldPixelStabilityPolicy pixelStabilityPolicy_;
        WorldMapMode mapMode_ = WorldMapMode::Political;
        mutable bool pixelStabilityActive_ = false;
    };
} // namespace Paladin
