#pragma once
#include "rendering/Texture.h"
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
        const TileRenderMetrics&
    ) const;

  private:
    struct Chunk
    {
        std::unique_ptr<Texture> texture;
        std::uint64_t version = 0;
    };
    mutable std::uint64_t sourceInstance_ = 0;
    mutable std::size_t refreshCursor_ = 0;
    mutable std::vector<Chunk> chunks_;
};
} // namespace Paladin
