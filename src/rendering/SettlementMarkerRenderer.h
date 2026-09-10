#pragma once

#include "rendering/SettlementWorldPresentation.h"
#include "ui/BitmapFontRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class Settlement;
    class World;

    struct TileRenderMetrics;

    class SettlementMarkerRenderer
    {
    public:
        SettlementMarkerRenderer() = default;

        explicit SettlementMarkerRenderer(
            SettlementWorldPresentationPolicy policy
        ) noexcept
            : policy_(policy)
        {
        }

        void renderFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            float visibility = 1.0F
        ) const;

        void renderGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            float visibility = 1.0F
        ) const;

        // WorldObjectRenderer owns projection/LOD now. Keep the established
        // procedural symbol as the temporary regional marker and close-world
        // settlement fallback until authored 32-pixel world sprites replace it.
        void drawAt(
            Renderer& renderer,
            const World& world,
            const Settlement& settlement,
            float centerX,
            float centerY,
            float visibility = 1.0F
        ) const
        {
            drawMarker(
                renderer,
                world,
                settlement,
                centerX,
                centerY,
                visibility
            );
        }

    private:
        void drawMarker(
            Renderer& renderer,
            const World& world,
            const Settlement& settlement,
            float centerX,
            float centerY,
            float visibility = 1.0F
        ) const;

        SettlementWorldPresentationPolicy policy_;
        BitmapFontRenderer fontRenderer_;
    };
} // namespace Paladin
