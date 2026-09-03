#pragma once

namespace Paladin
{
    class Renderer;
    class WorldGrid;
    class Camera2D;

    struct TileRenderMetrics;

    class WorldGridRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;
    };
}