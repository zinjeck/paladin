#pragma once

#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class Settlement;

    struct TileRenderMetrics;

    class SettlementMarkerRenderer
    {
    public:
        void render(
            Renderer& renderer,
            std::span<const Settlement> settlements,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;
    };
}
