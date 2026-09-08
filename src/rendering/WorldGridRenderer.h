#pragma once

#include "rendering/Renderer.h"
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    class Renderer;
    class Texture;
    struct SceneSprite;
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
        void reset() const;
        void renderGlobeTerrain(
            Renderer&,
            const WorldGrid&,
            const Camera2D&,
            const TileRenderMetrics&,
            const SceneSpriteLibrary&,
            const std::function<void(const Texture&, int, int)>&
        ) const;

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
            const SceneSpriteLibrary& sprites,
            const std::function<void(const Texture&, int, int)>& project = {}
        ) const;

        struct TerrainChunk
        {
            std::unique_ptr<Texture> texture;
            std::uint64_t lastUsed = 0;
            double readyAt = 0;
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
        mutable std::unordered_map<
            std::uint64_t,
            std::
                unordered_map<const SceneSprite*, std::array<RenderColor, 256>>>
            coastPaint_;
        struct CoastFields
        {
            std::array<std::array<double, 3>, 320> values{};
            std::bitset<320> ready;
            double readyAt = 0;
        };
        mutable std::unordered_map<std::uint64_t, CoastFields> coastFields_;


        mutable const void* cachedGrid_ = nullptr;
        mutable std::unique_ptr<Texture> cachedTerrainTexture_;
        mutable bool cacheBuildAttempted_ = false;
        struct OverviewData
        {
            int width = 0, height = 0;
            std::vector<RenderColor> pixels;
        };
        mutable std::future<OverviewData> overviewPending_;
        mutable std::shared_ptr<std::atomic_bool> overviewCancelled_;
        mutable OverviewData overviewReady_;
        mutable std::unique_ptr<Texture> overviewUpload_;
        mutable int overviewUploadRow_ = 0;
    };
} // namespace Paladin
