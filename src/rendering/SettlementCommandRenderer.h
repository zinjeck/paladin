#pragma once

#include "rendering/OverlayRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SettlementCommandController;
    class SettlementCommandState;
    struct TileRenderMetrics;

    class SettlementCommandRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const SettlementCommandState& state,
            const SettlementCommandController& controller,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

    private:
        OverlayRenderer overlayRenderer_;
    };
}
