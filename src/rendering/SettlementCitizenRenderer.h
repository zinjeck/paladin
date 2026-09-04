#pragma once

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SettlementCitizenState;
    struct TileRenderMetrics;

    class SettlementCitizenRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const SettlementCitizenState& citizens,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;
    };
}
