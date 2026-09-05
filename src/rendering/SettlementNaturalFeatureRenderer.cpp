#include "rendering/SettlementNaturalFeatureRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    namespace
    {
        // Replaceable presentation mask, independent of feature identity and logic.
        constexpr int pixelsPerTile = 10;
        constexpr int spriteSide = 13;
        constexpr int canopyRadius[spriteSide] = {1, 2, 4, 5, 5, 6, 6, 6, 5, 5, 4, 2, 1};
        bool canopy(int x, int y)
        {
            return y >= 0 && y < spriteSide && std::abs(x - 6) <= canopyRadius[y];
        }
    }
    void SettlementNaturalFeatureRenderer::render(Renderer& renderer,
        const SettlementMap& map, const Camera2D& camera,
        const TileRenderMetrics& metrics) const
    {
        constexpr int side = SettlementNaturalFeatures::ChunkSide;
        constexpr int padding = 2;
        constexpr int textureSide = side * pixelsPerTile + padding * 2;
        const int columns = (map.grid().width() + side - 1) / side;
        const int rows = (map.grid().height() + side - 1) / side;
        if (sourceInstance_ != map.instanceId())
        {
            sourceInstance_ = map.instanceId();
            chunks_.clear();
            chunks_.resize(std::size_t(columns) * rows);
        }
        const double tp = metrics.scaledTilePixels(camera.zoom());
        const double originX = renderer.outputWidth() * .5 - camera.tileX() * tp;
        const double originY = renderer.outputHeight() * .5 - camera.tileY() * tp;
        const int firstX = std::clamp(int(std::floor((-originX / tp - 1) / side)), 0, columns);
        const int lastX = std::clamp(int(std::ceil(((renderer.outputWidth() - originX) / tp + 1) / side)), 0, columns);
        const int firstY = std::clamp(int(std::floor((-originY / tp - 1) / side)), 0, rows);
        const int lastY = std::clamp(int(std::ceil(((renderer.outputHeight() - originY) / tp + 1) / side)), 0, rows);
        for (int cy = firstY; cy < lastY; ++cy)
        {
            for (int cx = firstX; cx < lastX; ++cx)
            {
                auto& chunk = chunks_[std::size_t(cy) * columns + cx];
                const auto version = map.naturalFeatures().chunkVersion(cx, cy);
                if (chunk.version != version)
                {
                    std::vector<RenderColor> pixels(textureSide * textureSide, {0, 0, 0, 0});
                    for (int y = 0; y < side; ++y)
                    {
                        for (int x = 0; x < side; ++x)
                        {
                            const SettlementTilePosition tile{cx * side + x, cy * side + y};
                            const auto feature = map.naturalFeatures().at(tile);
                            if (feature.kind == NaturalFeatureKind::None) continue;
                            const auto biome = map.grid().tile(tile)->biome;
                            const auto variation = GenerationNoise::mix(map.generationSeed()
                                ^ (std::uint64_t(tile.x) << 32) ^ std::uint64_t(tile.y));
                            RenderColor fill = feature.kind == NaturalFeatureKind::Rock
                                ? RenderColor{155, 157, 162, 255}
                                : biome == BiomeType::Taiga ? RenderColor{62, 112, 85, 255}
                                : biome == BiomeType::Jungle ? RenderColor{32, 117, 43, 255}
                                : RenderColor{67, 153, 62, 255};
                            const double shade = .92 + double(variation % 17) * .01;
                            fill.red = std::uint8_t(fill.red * shade);
                            fill.green = std::uint8_t(fill.green * shade);
                            fill.blue = std::uint8_t(fill.blue * shade);
                            const int rockX =
                                feature.kind == NaturalFeatureKind::Rock
                                    ? int((variation >> 8) % 3) - 1
                                    : 0;
                            const int rockY =
                                feature.kind == NaturalFeatureKind::Rock
                                    ? int((variation >> 16) % 3) - 1
                                    : 0;
                            const RenderColor border = feature.marked
                                ? RenderColor{255, 215, 50, 255}
                                : feature.kind == NaturalFeatureKind::Rock
                                    ? RenderColor{70, 72, 78, 255}
                                    : RenderColor{18, 57, 25, 255};
                            for (int sy = 0; sy < spriteSide; ++sy)
                            {
                                for (int sx = 0; sx < spriteSide; ++sx)
                                {
                                    const auto shape = [&](int px, int py)
                                    {
                                        return feature.kind ==
                                                       NaturalFeatureKind::Tree
                                                   ? canopy(px, py)
                                                   : [&]()
                                        {
                                            constexpr unsigned masks[4][7] = {
                                                {0x0C,
                                                 0x3E,
                                                 0x7E,
                                                 0x7F,
                                                 0x7F,
                                                 0x3F,
                                                 0x1C},
                                                {0x18,
                                                 0x3C,
                                                 0x7E,
                                                 0x7F,
                                                 0x3F,
                                                 0x3E,
                                                 0x0C},
                                                {0x1C,
                                                 0x3E,
                                                 0x7F,
                                                 0x7F,
                                                 0x7E,
                                                 0x3C,
                                                 0x18},
                                                {0x08,
                                                 0x1C,
                                                 0x3E,
                                                 0x7F,
                                                 0x7F,
                                                 0x3E,
                                                 0x1E}
                                            };
                                            const int rx = px - 3, ry = py - 3;
                                            return rx >= 0 && rx < 7 &&
                                                   ry >= 0 && ry < 7 &&
                                                   (masks[variation % 4][ry] &
                                                    (1u << rx));
                                        }();
                                    };
                                    if (!shape(sx, sy)) continue;
                                    const bool edge = !shape(sx - 1, sy) || !shape(sx + 1, sy)
                                        || !shape(sx, sy - 1) || !shape(sx, sy + 1);
                                    const int px = padding + x * pixelsPerTile + sx - 1 + rockX;
                                    const int py = padding + y * pixelsPerTile + sy - 1 + rockY;
                                    pixels[std::size_t(py) * textureSide + px] = edge ? border : fill;
                                }
                            }
                        }
                    }
                    chunk.texture = renderer.createTextureFromPixels(textureSide, textureSide, pixels);
                    chunk.version = version;
                }
                if (chunk.texture)
                    renderer.drawTexture(*chunk.texture, 0, 0, textureSide, textureSide,
                        float(originX + (cx * side - double(padding) / pixelsPerTile) * tp),
                        float(originY + (cy * side - double(padding) / pixelsPerTile) * tp),
                        float(textureSide * tp / pixelsPerTile),
                        float(textureSide * tp / pixelsPerTile));
            }
        }
    }
}
