#include "rendering/SettlementNaturalFeatureRenderer.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    namespace
    {
        // Replaceable presentation mask, independent of feature identity and
        // logic.
        constexpr int pixelsPerTile = 10;
        constexpr int spriteSide = 13;
        constexpr int canopyRadius[spriteSide] =
            {1, 2, 4, 5, 5, 6, 6, 6, 5, 5, 4, 2, 1};
        bool canopy(int x, int y)
        {
            return y >= 0 && y < spriteSide &&
                   std::abs(x - 6) <= canopyRadius[y];
        }
        const auto& spriteMasks()
        {
            static const auto masks = []
            {
                std::array<std::array<std::uint8_t, spriteSide * spriteSide>, 5>
                    result{};
                constexpr unsigned rocks[4][7] = {
                    {0x0C, 0x3E, 0x7E, 0x7F, 0x7F, 0x3F, 0x1C},
                    {0x18, 0x3C, 0x7E, 0x7F, 0x3F, 0x3E, 0x0C},
                    {0x1C, 0x3E, 0x7F, 0x7F, 0x7E, 0x3C, 0x18},
                    {0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x3E, 0x1E}
                };
                for (int kind = 0; kind < 5; ++kind)
                {
                    const auto shape = [&](int x, int y)
                    {
                        if (kind == 0)
                        {
                            return canopy(x, y);
                        }
                        const int rx = x - 3, ry = y - 3;
                        return rx >= 0 && rx < 7 && ry >= 0 && ry < 7 &&
                               (rocks[kind - 1][ry] & (1u << rx));
                    };
                    for (int y = 0; y < spriteSide; ++y)
                    {
                        for (int x = 0; x < spriteSide; ++x)
                        {
                            if (!shape(x, y))
                            {
                                continue;
                            }
                            const bool edge =
                                !shape(x - 1, y) || !shape(x + 1, y) ||
                                !shape(x, y - 1) || !shape(x, y + 1);
                            result[kind][y * spriteSide + x] = edge ? 2 : 1;
                        }
                    }
                }
                return result;
            }();
            return masks;
        }
    } // namespace
    void SettlementNaturalFeatureRenderer::render(
        Renderer& renderer,
        const SettlementMap& map,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        SceneDrawQueue* shared,
        const SceneSpriteLibrary* sprites,
        const CityPresentation* policy
    ) const
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
            refreshCursor_ = 0;
            chunks_.resize(std::size_t(columns) * rows);
        }
        const double tp = metrics.scaledTilePixels(camera.zoom());
        if (shared && sprites && policy && tp >= policy->detailTilePixels)
        {
            submitDetailed(
                renderer,
                map,
                {camera.tileX(),
                 camera.tileY(),
                 tp,
                 renderer.outputWidth(),
                 renderer.outputHeight()},
                *shared,
                *sprites,
                *policy
            );
            return;
        }
        const double originX =
            renderer.outputWidth() * .5 - camera.tileX() * tp;
        const double originY =
            renderer.outputHeight() * .5 - camera.tileY() * tp;
        const int firstX =
            std::clamp(int(std::floor((-originX / tp - 1) / side)), 0, columns);
        const int lastX = std::clamp(
            int(
                std::ceil(((renderer.outputWidth() - originX) / tp + 1) / side)
            ),
            0,
            columns
        );
        const int firstY =
            std::clamp(int(std::floor((-originY / tp - 1) / side)), 0, rows);
        const int lastY = std::clamp(
            int(
                std::ceil(((renderer.outputHeight() - originY) / tp + 1) / side)
            ),
            0,
            rows
        );
        const int visibleColumns = lastX - firstX, visibleRows = lastY - firstY;
        const std::size_t visibleCount =
            std::size_t(visibleColumns) * visibleRows;
        std::size_t rebuilt = 0;
        // Retain old textures while a bounded, rotating refresh catches up.
        // Simulation designations take effect immediately, independently.
        for (std::size_t scan = 0; scan < visibleCount && rebuilt < 8; ++scan)
        {
            const auto next = refreshCursor_++ % visibleCount;
            const int cx = firstX + int(next % visibleColumns),
                      cy = firstY + int(next / visibleColumns);
            auto& chunk = chunks_[std::size_t(cy) * columns + cx];
            const auto version = map.naturalFeatures().chunkVersion(cx, cy);
            if (chunk.version != version)
            {
                if (map.naturalFeatures().countIn(
                        {{cx * side, cy * side}, side, side}
                    ) == 0)
                {
                    chunk.texture.reset();
                    chunk.version = version;
                    continue;
                }
                ++rebuilt;
                std::vector<RenderColor> pixels(
                    textureSide * textureSide,
                    {0, 0, 0, 0}
                );
                for (int y = 0; y < side; ++y)
                {
                    for (int x = 0; x < side; ++x)
                    {
                        const SettlementTilePosition tile{
                            cx * side + x,
                            cy * side + y
                        };
                        const auto feature = map.naturalFeatures().at(tile);
                        if (feature.kind == NaturalFeatureKind::None)
                        {
                            continue;
                        }
                        const auto biome = map.grid().tile(tile)->biome;
                        const auto variation = GenerationNoise::mix(
                            map.generationSeed() ^
                            (std::uint64_t(tile.x) << 32) ^
                            std::uint64_t(tile.y)
                        );
                        RenderColor fill =
                            feature.kind == NaturalFeatureKind::Rock
                                ? RenderColor{155, 157, 162, 255}
                            : biome == BiomeType::Taiga
                                ? RenderColor{62, 112, 85, 255}
                            : biome == BiomeType::Jungle
                                ? RenderColor{32, 117, 43, 255}
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
                        const RenderColor border =
                            feature.marked ? RenderColor{255, 215, 50, 255}
                            : feature.kind == NaturalFeatureKind::Rock
                                ? RenderColor{70, 72, 78, 255}
                                : RenderColor{18, 57, 25, 255};
                        for (int sy = 0; sy < spriteSide; ++sy)
                        {
                            for (int sx = 0; sx < spriteSide; ++sx)
                            {
                                const auto mask = spriteMasks()
                                    [feature.kind == NaturalFeatureKind::Tree
                                         ? 0
                                         : 1 + variation % 4]
                                    [sy * spriteSide + sx];
                                if (!mask)
                                {
                                    continue;
                                }
                                const bool edge = mask == 2;
                                const int px = padding + x * pixelsPerTile +
                                               sx - 1 + rockX;
                                const int py = padding + y * pixelsPerTile +
                                               sy - 1 + rockY;
                                pixels[std::size_t(py) * textureSide + px] =
                                    edge ? border : fill;
                            }
                        }
                    }
                }
                chunk.texture = renderer.createTextureFromPixels(
                    textureSide,
                    textureSide,
                    pixels
                );
                chunk.version = version;
            }
        }
        for (int cy = firstY; cy < lastY; ++cy)
        {
            for (int cx = firstX; cx < lastX; ++cx)
            {
                const auto& chunk = chunks_[std::size_t(cy) * columns + cx];
                if (chunk.texture)
                {
                    renderer.drawTexture(
                        *chunk.texture,
                        0,
                        0,
                        textureSide,
                        textureSide,
                        float(
                            originX +
                            (cx * side - double(padding) / pixelsPerTile) * tp
                        ),
                        float(
                            originY +
                            (cy * side - double(padding) / pixelsPerTile) * tp
                        ),
                        float(textureSide * tp / pixelsPerTile),
                        float(textureSide * tp / pixelsPerTile)
                    );
                }
            }
        }
    }
    void SettlementNaturalFeatureRenderer::submitDetailed(
        Renderer& renderer,
        const SettlementMap& map,
        const SceneProjection& projection,
        SceneDrawQueue& queue,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy
    ) const
    {
        // Reuse the pre-existing tree/rock masks and palette, packed once.
        constexpr int variants = 7, shades = 17, frames = variants * shades * 2;
        constexpr int atlasWidth = frames * spriteSide;
        if (!placeholderAtlas_)
        {
            std::vector<RenderColor> pixels(
                atlasWidth * spriteSide,
                {0, 0, 0, 0}
            );
            for (int variant = 0; variant < variants; ++variant)
            {
                for (int shade = 0; shade < shades; ++shade)
                {
                    for (int marked = 0; marked < 2; ++marked)
                    {
                        const int frame =
                            (variant * shades + shade) * 2 + marked;
                        RenderColor fill =
                            variant >= 3   ? RenderColor{155, 157, 162, 255}
                            : variant == 1 ? RenderColor{62, 112, 85, 255}
                            : variant == 2 ? RenderColor{32, 117, 43, 255}
                                           : RenderColor{67, 153, 62, 255};
                        const double factor = .92 + shade * .01;
                        fill.red = std::uint8_t(fill.red * factor);
                        fill.green = std::uint8_t(fill.green * factor);
                        fill.blue = std::uint8_t(fill.blue * factor);
                        const RenderColor border =
                            marked         ? RenderColor{255, 215, 50, 255}
                            : variant >= 3 ? RenderColor{70, 72, 78, 255}
                                           : RenderColor{18, 57, 25, 255};
                        for (int y = 0; y < spriteSide; ++y)
                        {
                            for (int x = 0; x < spriteSide; ++x)
                            {
                                const auto mask = spriteMasks()
                                    [variant >= 3 ? variant - 2 : 0]
                                    [y * spriteSide + x];
                                if (mask)
                                {
                                    pixels
                                        [y * atlasWidth + frame * spriteSide +
                                         x] = mask == 2 ? border : fill;
                                }
                            }
                        }
                    }
                }
            }
            placeholderAtlas_ = renderer.createTextureFromPixels(
                atlasWidth,
                spriteSide,
                pixels
            );
        }
        if (!placeholderAtlas_)
        {
            return;
        }
        constexpr int side = SettlementNaturalFeatures::ChunkSide;
        const int columns = (map.grid().width() + side - 1) / side;
        const int rows = (map.grid().height() + side - 1) / side;
        // Catalog bounds are limited to 64 tiles, including pivot/elevation.
        // Expand candidate chunks so offscreen bases cannot clip tall artwork.
        const double pad =
            (sprites.find("tree") || sprites.find("rock")) ? 128 : 2;
        const double halfW =
            projection.screenWidth * .5 / projection.tilePixels;
        const double halfH =
            projection.screenHeight * .5 / projection.tilePixels;
        const int x0 = std::clamp(
            int(std::floor((projection.cameraX - halfW - pad) / side)),
            0,
            columns
        );
        const int y0 = std::clamp(
            int(std::floor((projection.cameraY - halfH - pad) / side)),
            0,
            rows
        );
        const int x1 = std::clamp(
            int(std::ceil((projection.cameraX + halfW + pad) / side)),
            0,
            columns
        );
        const int y1 = std::clamp(
            int(std::ceil((projection.cameraY + halfH + pad) / side)),
            0,
            rows
        );
        for (int cy = y0; cy < y1; ++cy)
        {
            for (int cx = x0; cx < x1; ++cx)
            {
                auto& chunk = chunks_[std::size_t(cy) * columns + cx];
                const auto version = map.naturalFeatures().chunkVersion(cx, cy);
                if (chunk.spriteVersion != version)
                {
                    chunk.sprites.clear();
                    if (map.naturalFeatures().countIn(
                            {{cx * side, cy * side}, side, side}
                        ))
                    {
                        for (int y = cy * side;
                             y < std::min((cy + 1) * side, map.grid().height());
                             ++y)
                        {
                            for (int x = cx * side;
                                 x <
                                 std::min((cx + 1) * side, map.grid().width());
                                 ++x)
                            {
                                const auto f = map.naturalFeatures().at({x, y});
                                if (f.kind == NaturalFeatureKind::None)
                                {
                                    continue;
                                }
                                const auto variation = GenerationNoise::mix(
                                    map.generationSeed() ^
                                    (std::uint64_t(x) << 32) ^ std::uint64_t(y)
                                );
                                const auto biome =
                                    map.grid().tile({x, y})->biome;
                                const bool tree =
                                    f.kind == NaturalFeatureKind::Tree;
                                const int variant =
                                    !tree ? 3 + int(variation % 4)
                                    : biome == BiomeType::Taiga  ? 1
                                    : biome == BiomeType::Jungle ? 2
                                                                 : 0;
                                chunk.sprites.push_back(
                                    {{x, y},
                                     (variant * shades + int(variation % 17)) *
                                             2 +
                                         int(f.marked),
                                     tree}
                                );
                            }
                        }
                    }
                    chunk.spriteVersion = version;
                }
                for (const auto& f : chunk.sprites)
                {
                    const double x = f.tile.x + .5, y = f.tile.y + .5;
                    const auto id =
                        ((std::uint64_t(f.tile.y) * map.grid().width() +
                          f.tile.x)
                         << 3) |
                        4;
                    const bool custom = sprites.submit(
                        queue,
                        projection,
                        f.tree ? "tree" : "rock",
                        x,
                        y,
                        id
                    );
                    const auto b = projection.bounds(
                        {x,
                         y,
                         f.tree ? policy.treeElevation : 0,
                         1.3,
                         1.3,
                         .5,
                         .5}
                    );
                    if (!projection.visible(b))
                    {
                        continue;
                    }
                    if (policy.shadowsVisible)
                    {
                        queue.submit(
                            {projection.bounds(
                                 {x + .15, y + .15, 0, 1, .35, .5, .5}
                             ),
                             policy.shadowColor,
                             y,
                             id,
                             -1}
                        );
                    }
                    if (!custom)
                    {
                        queue.submit(
                            {b,
                             {},
                             y,
                             id,
                             0,
                             0,
                             placeholderAtlas_.get(),
                             {float(f.frame * spriteSide),
                              0,
                              spriteSide,
                              spriteSide}}
                        );
                    }
                    if (custom && f.frame % 2)
                    {
                        queue.submit(
                            {projection.bounds({x, y, 0, 1, .1, .5, .5}),
                             {255, 215, 50, 255},
                             y,
                             id,
                             2,
                             0}
                        );
                    }
                }
            }
        }
    }
} // namespace Paladin
