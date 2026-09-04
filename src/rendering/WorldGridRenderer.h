#pragma once

#include <memory>

namespace Paladin
{
    class Renderer;
    class Texture;
    class WorldGrid;
    class Camera2D;

    struct TileRenderMetrics;

    class WorldGridRenderer
    {
    public:
        WorldGridRenderer();
        ~WorldGridRenderer();

        WorldGridRenderer(const WorldGridRenderer&) = delete;
        WorldGridRenderer& operator=(
            const WorldGridRenderer&
        ) = delete;

        void render(
            Renderer& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

    private:
        mutable const WorldGrid* cachedGrid_ = nullptr;
        mutable std::unique_ptr<Texture> cachedTerrainTexture_;
        mutable bool cacheBuildAttempted_ = false;
    };
}
