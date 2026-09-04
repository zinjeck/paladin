#pragma once

#include "rendering/OverlayRenderer.h"
#include "rendering/SettlementMarkerRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/TerritoryPresentationPolicy.h"
#include "rendering/TerritoryRenderer.h"
#include "rendering/WorldGridRenderer.h"

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
        WorldRenderer();

        explicit WorldRenderer(
            TerritoryPresentationPolicy territoryPresentationPolicy
        );

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            std::span<const SpriteRenderItem> sprites = {},
            std::span<const TileOverlayRenderItem> overlays = {},
            std::span<const TileOutlineRenderItem> outlines = {}
        ) const;

    private:
        WorldGridRenderer gridRenderer_;
        TerritoryRenderer territoryRenderer_;
        SpriteRenderer spriteRenderer_;
        SettlementMarkerRenderer settlementMarkerRenderer_;
        OverlayRenderer overlayRenderer_;
        TerritoryPresentationPolicy territoryPresentationPolicy_;
        mutable bool politicalViewActive_ = false;
        mutable bool politicalViewInitialized_ = false;
    };
}
