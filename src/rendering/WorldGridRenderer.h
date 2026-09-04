#pragma once

#include <memory>

namespace Paladin
{
    class Renderer;
    class Texture;
    class WorldGrid;
    class SettlementGrid;
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

        void render(
            Renderer& renderer,
            const SettlementGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

        // Reuses the terrain texture populated by render().
        void renderOverview(
            Renderer& renderer,
            float x, float y, float width, float height
        ) const;

    private:
        template<typename Grid>
        void renderGrid(
            Renderer& renderer,
            const Grid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

        mutable const void* cachedGrid_ = nullptr;
        mutable std::unique_ptr<Texture> cachedTerrainTexture_;
        mutable bool cacheBuildAttempted_ = false;
    };
}
