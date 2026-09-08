#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/Texture.h"
#include "world/SettlementTilePosition.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
namespace Paladin
{
    class Renderer;
    class SettlementMap;
    class Camera2D;
    struct TileRenderMetrics;
    class SettlementNaturalFeatureRenderer
    {
    public:
        void invalidateArt() const
        {
            clearanceInstance_ = 0;
            sourceInstance_ = 0;
        }
        void render(
            Renderer&,
            const SettlementMap&,
            const Camera2D&,
            const TileRenderMetrics&,
            SceneDrawQueue* = nullptr,
            const SceneSpriteLibrary* = nullptr,
            const CityPresentation* = nullptr
        ) const;

    private:
        void submitDetailed(
            Renderer&,
            const SettlementMap&,
            const SceneProjection&,
            SceneDrawQueue&,
            const SceneSpriteLibrary&,
            const CityPresentation&
        ) const;
        struct FeatureSprite
        {
            SettlementTilePosition tile;
            int frame;
            bool tree;
        };
        struct Chunk
        {
            std::unique_ptr<Texture> texture;
            std::uint64_t version = 0;
            std::uint64_t baseVersion = 0, navigation = 0;
            bool empty = false, textureDirty = false;
            std::vector<TextureDrawItem> commands;
            std::unique_ptr<PreparedQuadMesh> mesh;
            bool batchable = false;
            std::uint64_t spriteVersion = ~std::uint64_t(0);
            std::vector<FeatureSprite> sprites;
        };
        mutable std::unique_ptr<Texture> placeholderAtlas_;
        mutable std::unique_ptr<Texture> contactShadow_;
        mutable std::unique_ptr<Texture> foliageAtlas_;
        mutable std::unordered_map<const Texture*, RenderRectangle>
            foliageSources_;
        mutable std::vector<MeshVertex> fallbackVertices_;
        mutable std::vector<int> fallbackIndices_;
        mutable RenderColor contactShadowColor_{0, 0, 0, 0};
        mutable std::uint64_t sourceInstance_ = 0;
        mutable bool overviewArt_ = false;
        mutable std::uint64_t navigationSource_ = ~std::uint64_t(0);
        mutable std::vector<std::uint64_t> navigationKeys_;
        mutable std::size_t refreshCursor_ = 0, cachedTextures_ = 0;
        mutable std::vector<Chunk> chunks_, featureChunks_;
        mutable std::unique_ptr<Texture> overviewTexture_;
        mutable std::uint64_t clearanceInstance_ = 0,
                              clearanceVersion_ = ~std::uint64_t(0);
        mutable bool clearanceArtEnabled_ = false;
        mutable std::unordered_map<std::uint64_t, std::vector<RenderRectangle>>
            clearance_;
    };
} // namespace Paladin
