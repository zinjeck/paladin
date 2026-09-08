#pragma once

#include "rendering/Renderer.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    class Renderer;
    class Texture;
    class WorldGrid;
    class SettlementGrid;
    class Camera2D;
    class SceneSpriteLibrary;

    struct TileRenderMetrics;

    class WorldGridRenderer
    {
    public:
        WorldGridRenderer();
        ~WorldGridRenderer();

        WorldGridRenderer(const WorldGridRenderer&) = delete;
        WorldGridRenderer& operator=(const WorldGridRenderer&) = delete;

        void render(
            Renderer& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary* sprites = nullptr
        ) const;

        void render(
            Renderer& renderer,
            const SettlementGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary* sprites = nullptr
        ) const;

        // Reuses the terrain texture populated by render().
        void renderOverview(
            Renderer& renderer,
            float x,
            float y,
            float width,
            float height
        ) const;

    private:
        template<typename Grid>
        void renderCoast(
            Renderer&,
            const Grid&,
            const Camera2D&,
            const TileRenderMetrics&,
            const SceneSpriteLibrary&
        ) const;
        template<typename Grid>
        void renderGrid(
            Renderer& renderer,
            const Grid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary* sprites
        ) const;

        template<typename Grid>
        void renderSpriteTerrain(
            Renderer& renderer,
            const Grid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary& sprites
        ) const;

        struct TerrainChunk
        {
            std::unique_ptr<Texture> texture;
            std::uint64_t lastUsed = 0;
            int resolution = 0;
            std::vector<TextureDrawItem> commands;
        };
        // Strong references make an artwork reload detectable even if an
        // allocator would otherwise reuse the previous texture's address.
        mutable std::vector<std::shared_ptr<Texture>> terrainSources_;
        mutable std::vector<double> terrainDimensions_;
        mutable std::unordered_map<std::uint64_t, TerrainChunk> terrainChunks_;
        mutable int terrainPixelsPerTile_ = 0;
        mutable std::uint64_t terrainFrame_ = 0;
        mutable std::size_t terrainBytes_ = 0;

        mutable const void* cachedGrid_ = nullptr;
        mutable std::unique_ptr<Texture> cachedTerrainTexture_;
        mutable bool cacheBuildAttempted_ = false;
    };
} // namespace Paladin
