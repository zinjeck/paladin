#pragma once
#include "rendering/Texture.h"
#include <memory>
#include <vector>
#include <cstdint>
namespace Paladin
{
    class Renderer;
    class SettlementMap;
    class Camera2D;
    struct TileRenderMetrics;
    class SettlementNaturalFeatureRenderer
    {
    public:
        void render(Renderer&, const SettlementMap&, const Camera2D&,
            const TileRenderMetrics&) const;
    private:
        struct Chunk
        {
            std::unique_ptr<Texture> texture;
            std::uint64_t version = 0;
        };
        mutable const SettlementMap* source_ = nullptr;
        mutable std::vector<Chunk> chunks_;
    };
}
