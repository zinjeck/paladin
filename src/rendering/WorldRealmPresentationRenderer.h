#pragma once

#include "rendering/TribalInfluenceRenderer.h"
#include "rendering/WorldTerritoryPresentationRenderer.h"

namespace Paladin
{
    // One façade preserves WorldRenderer's existing call sites while selecting
    // the correct territorial language for each realm. Tribal authority is
    // painted first as a continuous field. Civic control is painted second so
    // formal sovereignty wins any overlap. The final relief pass adds only a
    // restrained inward civic shade and does not alter control or borders.
    class WorldRealmPresentationRenderer
    {
    public:
        void reset()
        {
            tribal_.reset();
            civic_.reset();
        }

        void renderFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        )
        {
            tribal_.renderTribalFlat(
                renderer,
                world,
                camera,
                metrics,
                presentation,
                policy
            );
            civic_.renderFlat(
                renderer,
                world,
                camera,
                metrics,
                presentation,
                policy
            );
            tribal_.renderCivicReliefFlat(
                renderer,
                world,
                camera,
                metrics,
                presentation
            );
        }

        void renderGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        )
        {
            tribal_.renderTribalGlobe(
                renderer,
                world,
                camera,
                presentation,
                policy
            );
            civic_.renderGlobe(
                renderer,
                world,
                camera,
                presentation,
                policy
            );
            tribal_.renderCivicReliefGlobe(
                renderer,
                world,
                camera,
                presentation
            );
        }

    private:
        TribalInfluenceRenderer tribal_;
        WorldTerritoryPresentationRenderer civic_;
    };
} // namespace Paladin
