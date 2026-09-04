#pragma once

#include "rendering/WorldGridRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SettlementMap;
    struct TileRenderMetrics;

    class CityRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

    private:
        WorldGridRenderer gridRenderer_;
    };
}
