#pragma once

#include "ui/BitmapFontRenderer.h"

#include <memory>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    struct TerritoryPresentationPolicy;
    struct TerritoryRendererCache;
    struct TileRenderMetrics;

    class TerritoryRenderer
    {
    public:
        TerritoryRenderer();
        ~TerritoryRenderer();

        TerritoryRenderer(const TerritoryRenderer&) = delete;
        TerritoryRenderer& operator=(
            const TerritoryRenderer&
        ) = delete;

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            bool politicalView,
            const TerritoryPresentationPolicy& policy
        ) const;

    private:
        BitmapFontRenderer fontRenderer_;
        mutable std::unique_ptr<TerritoryRendererCache> cache_;
    };
}
