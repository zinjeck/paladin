#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/Texture.h"
#include "world/SettlementTilePosition.h"
#include <cstdint>
#include <memory>
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
            std::uint64_t spriteVersion = ~std::uint64_t(0);
            std::vector<FeatureSprite> sprites;
        };
        mutable std::unique_ptr<Texture> placeholderAtlas_;
        mutable std::uint64_t sourceInstance_ = 0;
        mutable std::size_t refreshCursor_ = 0;
        mutable std::vector<Chunk> chunks_;
    };
} // namespace Paladin
