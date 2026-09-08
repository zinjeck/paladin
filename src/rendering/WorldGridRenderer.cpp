#include "rendering/WorldGridRenderer.h"
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/SceneDetail.h"
#include "rendering/TerrainMaterialField.h"
#include "rendering/WorldReliefPlacement.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/BiomeType.h"
#include "world/SettlementGrid.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace Paladin
{
    namespace
    {
        constexpr std::array<const char*, 10> terrainIds{
            "terrain.plain",
            "terrain.forest",
            "terrain.jungle",
            "terrain.desert",
            "terrain.tundra",
            "terrain.taiga",
            "terrain.water",
            "terrain.mountain",
            "terrain.shallow",
            "terrain.beach"
        };

        std::size_t terrainIndex(const WorldTile& tile)
        {
            if (tile.biome == BiomeType::Polar)
            {
                return 4;
            }
            if (tile.terrain == TerrainType::Water)
            {
                return 6;
            }
            if (tile.terrain == TerrainType::Mountain)
            {
                return 7;
            }
            if (tile.biome == BiomeType::Hills)
            {
                return 0;
            }
            return std::min(std::size_t(tile.biome), std::size_t(6));
        }

        template<typename Grid>
        bool mountainInterior(const Grid& grid, int x, int y)
        {
            if constexpr (!std::is_same_v<Grid, SettlementGrid>)
            {
                return false;
            }
            const auto mountain = [&](int xx, int yy)
            {
                const auto* t = grid.tile({xx, yy});
                return t && t->terrain == TerrainType::Mountain;
            };
            return mountain(x, y) && mountain(x - 1, y) && mountain(x + 1, y) &&
                   mountain(x, y - 1) && mountain(x, y + 1);
        }
        template<typename Grid> bool shoreWater(const Grid& grid, int x, int y)
        {
            const auto* t = grid.tile({x, y});
            if (!t || t->terrain != TerrainType::Water)
            {
                return false;
            }
            if constexpr (std::is_same_v<Grid, SettlementGrid>)
            {
                return grid.cityTileType({x, y}) == CityTileType::ShallowWater;
            }
            const auto land = [&](int xx, int yy)
            {
                const auto* n = grid.tile({xx, yy});
                return n && n->terrain != TerrainType::Water;
            };
            return land(x - 1, y) || land(x + 1, y) || land(x, y - 1) ||
                   land(x, y + 1);
        }
        RenderColor artOverviewColor(const WorldTile& tile)
        {
            // Approved Sunlight & Shadow base shades keep the strategic map
            // legible after its texture detail becomes smaller than a pixel.
            constexpr std::array<RenderColor, 8> colors{
                {{0x79, 0xB5, 0x6D, 255},
                 {0x49, 0x97, 0x5B, 255},
                 {0x33, 0x7A, 0x58, 255},
                 {0xD9, 0xC7, 0x9F, 255},
                 {0xAF, 0xC9, 0xD6, 255},
                 {0x4F, 0x8C, 0x7A, 255},
                 {0x54, 0x8A, 0xC4, 255},
                 {0x71, 0x6D, 0x70, 255}}
            };
            return colors[terrainIndex(tile)];
        }
        std::string terrainArtId(
            const WorldTile& tile,
            const SceneSpriteLibrary& sprites,
            bool worldScale = false
        )
        {
            std::string id = terrainIds[terrainIndex(tile)];
            if (worldScale)
            {
                if (tile.terrain == TerrainType::Mountain ||
                    tile.biome == BiomeType::Hills)
                {
                    // Relief exposes rock through the local climate's ground;
                    // hills never receive a universal green or brown carpet.
                    const auto t = tile.temperature.value(),
                               rain = tile.rainfall.value();
                    id = t >= .62 ? (rain < .24   ? "terrain.desert"
                                     : rain < .68 ? "terrain.plain"
                                                  : "terrain.jungle")
                         : t <= .34
                             ? (rain < .45 ? "terrain.tundra" : "terrain.taiga")
                         : rain < .42 ? "terrain.plain"
                                      : "terrain.forest";
                }
                return "world." + id;
            }
            if (tile.terrain == TerrainType::Land &&
                (tile.biome == BiomeType::Plain ||
                 tile.biome == BiomeType::Forest ||
                 tile.biome == BiomeType::Jungle ||
                 tile.biome == BiomeType::Taiga ||
                 tile.biome == BiomeType::Hills))
            {
                // Settlement temperatures are interpolated from the world's
                // latitude-dependent climate; local tile Y is not a fake
                // latitude.
                const double temperature = tile.temperature.value();
                const auto suffix = temperature > .68 ? ".warm"
                                    : temperature > 0 && temperature < .32
                                        ? ".cold"
                                        : "";
                if (*suffix && sprites.find(id + suffix))
                {
                    id += suffix;
                }
            }
            return id;
        }

        RenderColor biomeColor(BiomeType biome) noexcept
        {
            switch (biome)
            {
            case BiomeType::Plain:
                return {92, 166, 64, 255};
            case BiomeType::Hills:
                return {121, 181, 109, 255};

            case BiomeType::Forest:
                return {26, 107, 41, 255};

            case BiomeType::Jungle:
                return {5, 92, 23, 255};

            case BiomeType::Desert:
                return {219, 184, 92, 255};

            case BiomeType::Tundra:
                return {163, 184, 173, 255};

            case BiomeType::Polar:
                return {215, 224, 227, 255};

            case BiomeType::Taiga:
                return {51, 97, 82, 255};

            case BiomeType::Ocean:
                return {13, 41, 92, 255};
            }

            return {255, 0, 255, 255};
        }

        RenderColor tileColor(const WorldTile& tile) noexcept
        {
            if (tile.terrain == TerrainType::Water)
            {
                return biomeColor(BiomeType::Ocean);
            }

            if (tile.terrain == TerrainType::Mountain)
            {
                return {115, 107, 97, 255};
            }

            return biomeColor(tile.biome);
        }
        struct TerrainComposer
        {
            std::vector<TextureDrawItem>& items;
            int side;
            int outputWidth() const
            {
                return side;
            }
            int outputHeight() const
            {
                return side;
            }
            void fillRectangle(
                float x,
                float y,
                float w,
                float h,
                RenderColor color
            )
            {
                const float l = std::max(0.f, x), t = std::max(0.f, y),
                            r = std::min(float(side), x + w),
                            b = std::min(float(side), y + h);
                if (r > l && b > t)
                {
                    items.push_back(
                        {nullptr, {}, {l, t, r - l, b - t}, color, 255}
                    );
                }
            }
            void drawTexture(
                const Texture& t,
                float sx,
                float sy,
                float sw,
                float sh,
                float x,
                float y,
                float w,
                float h,
                std::uint8_t alpha = 255
            )
            {
                const float l = std::max(0.f, x), top = std::max(0.f, y),
                            r = std::min(float(side), x + w),
                            b = std::min(float(side), y + h);
                if (r > l && b > top)
                {
                    items.push_back(
                        {&t,
                         {sx + (l - x) * sw / w,
                          sy + (top - y) * sh / h,
                          (r - l) * sw / w,
                          (b - top) * sh / h},
                         {l, top, r - l, b - top},
                         {},
                         alpha}
                    );
                }
            }
        };
        template<class Target>
        void worldTerrainEdges(
            Target& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary& sprites
        )
        {
            const double tp = metrics.scaledTilePixels(camera.zoom());
            const double ox = renderer.outputWidth() * .5 - camera.tileX() * tp;
            const double oy =
                renderer.outputHeight() * .5 - camera.tileY() * tp;
            const int x0 = std::max(0, int(std::floor(-ox / tp))),
                      y0 = std::max(0, int(std::floor(-oy / tp)));
            const int x1 = std::min(
                grid.width(),
                int(std::ceil((renderer.outputWidth() - ox) / tp))
            );
            const int y1 = std::min(
                grid.height(),
                int(std::ceil((renderer.outputHeight() - oy) / tp))
            );
            const auto tile = [&](int x, int y)
            {
                return grid.tile(
                    {std::clamp(x, 0, grid.width() - 1),
                     std::clamp(y, 0, grid.height() - 1)}
                );
            };
            const auto land = [&](int x, int y)
            { return tile(x, y)->terrain != TerrainType::Water; };
            const auto mountain = [&](int x, int y)
            { return tile(x, y)->terrain == TerrainType::Mountain; };
            for (int y = y0; y < y1; ++y)
            {
                for (int x = x0; x < x1; ++x)
                {
                    const auto* current = tile(x, y);
                    bool coast = false, ridge = false;
                    const WorldTile* grass = current;
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            coast |= land(x + dx, y + dy) != land(x, y);
                            ridge |= mountain(x + dx, y + dy) != mountain(x, y);
                            if (tile(x + dx, y + dy)->terrain ==
                                TerrainType::Land)
                            {
                                grass = tile(x + dx, y + dy);
                            }
                        }
                    }
                    if (!coast)
                    {
                        continue;
                    }
                    const auto* green =
                        sprites.find(terrainArtId(*grass, sprites, true));
                    const auto* rock = sprites.find("terrain.mountain");
                    const auto material = [&](int col, int row)
                    {
                        const double xx = x + (col + .5) / 16,
                                     yy = y + (row + .5) / 16;
                        const auto sample = coastSample(xx, yy, true);
                        const double l = surfaceField(sample.x, sample.y, land);
                        if (l < .5)
                        {
                            return l > .40 ? 1 : 0;
                        }

                        // Sparse sand margin at strategic scale; no city-sized
                        // foam.
                        return l < .55 && std::sin(xx * .9 + yy * .7) > .2 ? 4
                                                                           : 2;
                    };
                    for (int row = 0; row < 16; ++row)
                    {
                        for (int col = 0; col < 16;)
                        {
                            const int first = col, m = material(col++, row);
                            while (col < 16 && material(col, row) == m)
                            {
                                ++col;
                            }
                            const double u = first / 16., v = row / 16.,
                                         width = (col - first) / 16.;
                            if (m < 2)
                            {
                                renderer.fillRectangle(
                                    float(ox + (x + u) * tp),
                                    float(oy + (y + v) * tp),
                                    float(width * tp),
                                    float(tp / 16),
                                    m == 0 ? RenderColor{0x20, 0x2C, 0x43, 255}
                                           : RenderColor{0x30, 0x45, 0x5D, 255}
                                );
                            }
                            else if (
                                const auto* art =
                                    m == 3 ? rock
                                    : m == 4
                                        ? sprites.find("world.terrain.beach")
                                        : green
                            )
                            {
                                for (int c = first; c < col; ++c)
                                {
                                    const auto color = landscapePaint(
                                        *art,
                                        x + (c + .5) / 16.,
                                        y + (row + .5) / 16.,
                                        true
                                    );
                                    renderer.fillRectangle(
                                        float(ox + (x + c / 16.) * tp),
                                        float(oy + (y + row / 16.) * tp),
                                        float(tp / 16),
                                        float(tp / 16),
                                        color
                                    );
                                }
                            }
                        }
                    }
                }
            }
        }
        template<class Target>
        void mountainRanges(
            Target& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary& sprites,
            bool includeHills = true
        )
        {
            const double tp = metrics.scaledTilePixels(camera.zoom());
            if (tp < 12 || !sprites.find("mountain.peak.a"))
            {
                return;
            }
            const double ox = renderer.outputWidth() * .5 - camera.tileX() * tp;
            const double oy =
                renderer.outputHeight() * .5 - camera.tileY() * tp;
            const int x0 = std::max(0, int(std::floor(-ox / tp)) - 5),
                      y0 = std::max(0, int(std::floor(-oy / tp)) - 5);
            const int x1 = std::min(
                grid.width(),
                int(std::ceil((renderer.outputWidth() - ox) / tp)) + 5
            );
            const int y1 = std::min(
                grid.height(),
                int(std::ceil((renderer.outputHeight() - oy) / tp)) + 5
            );
            // Stable irregular anchors. Different connected silhouettes overlap
            // into ranges; sparse foothills leave broad open slopes between
            // them.
            for (int row = y0 / 3 - 1; row <= y1 / 3 + 1; ++row)
            {
                for (int col = x0 / 4 - 1; col <= x1 / 4 + 1; ++col)
                {
                    const auto hash = landscapeHash(col, row, 431);
                    const int x = col * 4 + int(hash % 3),
                              y = row * 3 + int((hash >> 4) % 3);
                    const auto* tile = grid.tile({x, y});
                    if (!tile || tile->terrain == TerrainType::Water)
                    {
                        continue;
                    }
                    const bool hill = tile->terrain != TerrainType::Mountain &&
                                      tile->biome == BiomeType::Hills;
                    if (!hill && tile->terrain != TerrainType::Mountain)
                    {
                        continue;
                    }
                    if (hill && hash % 4 == 0)
                    {
                        continue;
                    }
                    if (hill && !includeHills)
                    {
                        continue;
                    }
                    bool core = !hill;
                    for (const auto& d : std::array<std::pair<int, int>, 4>{
                             {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}
                         })
                    {
                        const auto* neighbor =
                            grid.tile({x + d.first, y + d.second});
                        core &= neighbor &&
                                neighbor->terrain == TerrainType::Mountain;
                    }
                    const auto name =
                        std::string(
                            hill ? "world.relief.hill." : "world.relief.ridge."
                        ) +
                        std::to_string(1 + (hash >> 8) % 2);
                    const auto climate = terrainArtId(*tile, sprites, true);
                    const auto* art = sprites.find(
                        name + "." + climate.substr(climate.rfind('.') + 1)
                    );
                    if (!art)
                    {
                        art = sprites.find(name);
                    }
                    if (!art)
                    {
                        art = sprites.find(
                            hill ? "hill.mound" : "mountain.range.a"
                        );
                    }
                    if (!art)
                    {
                        continue;
                    }
                    const double xx = x - .5 + (hash % 17) / 34., yy = y - .5;
                    const double w = hill ? 3.0 + ((hash >> 12) % 13) / 10.
                                          : 3.7 + ((hash >> 12) % 17) / 10.;
                    const double h =
                        hill ? 1.6 + ((hash >> 18) % 9) / 10.
                             : (core ? 3.5 : 2.6) + ((hash >> 18) % 9) / 10.;
                    const auto frame = sprites.frame(*art, false);
                    // Cut only at coastlines, never draw a mountain across
                    // water.
                    for (int ty = int(std::floor(yy));
                         ty < int(std::ceil(yy + h));
                         ++ty)
                    {
                        for (int tx = int(std::floor(xx));
                             tx < int(std::ceil(xx + w));
                             ++tx)
                        {
                            const auto* under = grid.tile({tx, ty});
                            if (!under || under->terrain == TerrainType::Water)
                            {
                                continue;
                            }
                            const double l = std::max(xx, double(tx)),
                                         t = std::max(yy, double(ty));
                            const double r = std::min(xx + w, tx + 1.),
                                         b = std::min(yy + h, ty + 1.);
                            renderer.drawTexture(
                                *art->texture,
                                float(frame.x + (l - xx) / w * frame.width),
                                float(frame.y + (t - yy) / h * frame.height),
                                float((r - l) / w * frame.width),
                                float((b - t) / h * frame.height),
                                float(ox + l * tp),
                                float(oy + t * tp),
                                float((r - l) * tp),
                                float((b - t) * tp),
                                255
                            );
                        }
                    }
                }
            }
        }
    } // namespace


    WorldGridRenderer::WorldGridRenderer() = default;


    WorldGridRenderer::~WorldGridRenderer()
    {
        if (overviewCancelled_)
        {
            overviewCancelled_->store(true);
        }
    }
    void WorldGridRenderer::reset() const
    {
        if (overviewCancelled_)
        {
            overviewCancelled_->store(true);
        }
        cachedGrid_ = nullptr;
        cachedTerrainTexture_.reset();
        cacheBuildAttempted_ = false;
        overviewReady_ = {};
        overviewUpload_.reset();
        overviewUploadRow_ = 0;
        terrainChunks_.clear();
        coastPaint_.clear();
        coastFields_.clear();
        terrainSources_.clear();
        terrainDimensions_.clear();
        terrainBytes_ = 0;
    }
    void WorldGridRenderer::renderGlobeTerrain(
        Renderer& renderer,
        const WorldGrid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary& art,
        const std::function<void(const Texture&, int, int)>& project
    ) const
    {
        if (cachedGrid_ != &grid)
        {
            cachedGrid_ = &grid;
            terrainChunks_.clear();
            coastPaint_.clear();
            coastFields_.clear();
            terrainBytes_ = 0;
        }
        renderSpriteTerrain(renderer, grid, camera, metrics, art, project);
    }


    void WorldGridRenderer::render(
        Renderer& renderer,
        const WorldGrid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary* sprites
    ) const
    {
        renderGrid(renderer, grid, camera, metrics, sprites);
        if (sprites)
        {
            // Static coasts and relief are composed into the terrain cache.
        }
    }


    void WorldGridRenderer::render(
        Renderer& renderer,
        const SettlementGrid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary* sprites
    ) const
    {
        renderGrid(renderer, grid, camera, metrics, sprites);
        if (sprites)
        {
            renderCoast(renderer, grid, camera, metrics, *sprites);
        }
    }


    template<typename Grid>
    void WorldGridRenderer::renderCoast(
        Renderer& renderer,
        const Grid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary& sprites
    ) const
    {
        const double tp = metrics.scaledTilePixels(camera.zoom());
        if (tp <= 8 || !sprites.find("terrain.shallow") ||
            !sprites.find("terrain.beach"))
        {
            return;
        }
        std::vector<TextureDrawItem> draws;
        double coastTileOpacity = 1;
        const auto drawTexture = [&](const Texture& t,
                                     float sx,
                                     float sy,
                                     float sw,
                                     float sh,
                                     float x,
                                     float y,
                                     float w,
                                     float h,
                                     std::uint8_t opacity)
        {
            draws.push_back(
                {&t,
                 {sx, sy, sw, sh},
                 {x, y, w, h},
                 {},
                 std::uint8_t(opacity * coastTileOpacity)}
            );
        };
        const auto fillRectangle =
            [&](float x, float y, float w, float h, RenderColor c)
        {
            c.alpha = std::uint8_t(c.alpha * coastTileOpacity);
            draws.push_back({nullptr, {}, {x, y, w, h}, c, 255});
        };
        const double ox = renderer.outputWidth() * .5 - camera.tileX() * tp;
        const double oy = renderer.outputHeight() * .5 - camera.tileY() * tp;
        const int x0 = std::max(0, int(std::floor(-ox / tp))),
                  y0 = std::max(0, int(std::floor(-oy / tp)));
        const int x1 = std::min(
            grid.width(),
            int(std::ceil((renderer.outputWidth() - ox) / tp))
        );
        const int y1 = std::min(
            grid.height(),
            int(std::ceil((renderer.outputHeight() - oy) / tp))
        );
        const auto tileAt = [&](int x, int y)
        {
            return grid.tile(
                {std::clamp(x, 0, grid.width() - 1),
                 std::clamp(y, 0, grid.height() - 1)}
            );
        };
        const auto kind = [&](int x, int y)
        {
            if constexpr (std::is_same_v<Grid, SettlementGrid>)
            {
                return grid.cityTileType({x, y});
            }
            else
            {
                return shoreWater(grid, x, y) ? CityTileType::ShallowWater
                                              : CityTileType::DeepWater;
            }
        };
        const auto land = [&](int x, int y)
        { return tileAt(x, y)->terrain != TerrainType::Water; };
        const auto sand = [&](int x, int y)
        { return kind(x, y) == CityTileType::Beach; };
        const auto paint = [&](const SceneSprite* s,
                               int x,
                               int y,
                               double u,
                               double v,
                               double w,
                               double h,
                               int alpha = 255)
        {
            if (!s)
            {
                return;
            }
            if (s->materialPixels && s->materialWidth >= 16 && s->frames == 1 &&
                s != sprites.find("terrain.water") &&
                s != sprites.find("terrain.shallow"))
            {
                auto& variants = coastPaint_
                    [(std::uint64_t(std::uint32_t(x)) << 32) |
                     std::uint32_t(y)];
                auto found = variants.find(s);
                if (found == variants.end())
                {
                    std::array<RenderColor, 256> colors;
                    for (int cy = 0; cy < 16; ++cy)
                    {
                        for (int cx = 0; cx < 16; ++cx)
                        {
                            colors[cy * 16 + cx] = landscapePaint(
                                *s,
                                x + (cx + .5) / 16.,
                                y + (cy + .5) / 16.,
                                false
                            );
                        }
                    }
                    found = variants.emplace(s, std::move(colors)).first;
                }
                const int x0 = int(std::round(u * 16)),
                          x1 = int(std::round((u + w) * 16));
                const int y0 = int(std::round(v * 16)),
                          y1 = int(std::round((v + h) * 16));
                for (int py = y0; py < y1; ++py)
                {
                    for (int px = x0; px < x1; ++px)
                    {
                        auto color = found->second[py * 16 + px];
                        color.alpha = std::uint8_t(alpha);
                        fillRectangle(
                            float(ox + (x + px / 16.) * tp),
                            float(oy + (y + py / 16.) * tp),
                            float(tp / 16),
                            float(tp / 16),
                            color
                        );
                    }
                }
                return;
            }
            auto f = sprites.frame(*s, tp >= AnimationDetailPixels);
            const int mw = std::max(1, int(s->width)),
                      mh = std::max(1, int(s->height));
            if (tp >= AnimationDetailPixels && s->frames > 1 && s->fps > 0)
            {
                f.x = float(
                          int(sprites.time() * s->fps +
                              (x / mw * 13 + y / mh * 7) % s->frames) %
                          s->frames
                      ) *
                      f.width;
            }
            drawTexture(
                *s->texture,
                f.x + float((x % mw + u) * f.width / mw),
                f.y + float((y % mh + v) * f.height / mh),
                float(w * f.width / mw),
                float(h * f.height / mh),
                float(ox + (x + u) * tp),
                float(oy + (y + v) * tp),
                float(w * tp),
                float(h * tp),
                std::uint8_t(alpha)
            );
        };
        const auto coastDeadline = SDL_GetTicksNS() + 2000000;
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                if constexpr (std::is_same_v<Grid, SettlementGrid>)
                {
                    if (!grid.coastPassNeeded(x, y))
                    {
                        continue;
                    }
                }
                const auto coastKey =
                    (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y);
                auto cachedCoast = coastFields_.find(coastKey);
                if (cachedCoast == coastFields_.end())
                {
                    if (SDL_GetTicksNS() >= coastDeadline)
                    {
                        continue;
                    }
                    cachedCoast = coastFields_.try_emplace(coastKey).first;
                }
                coastTileOpacity = 1;
                const bool isLand = land(x, y);
                const auto* water = sprites.find(
                    kind(x, y) == CityTileType::DeepWater ? "terrain.water"
                                                          : "terrain.shallow"
                );
                if (!isLand)
                {
                    paint(water, x, y, 0, 0, 1, 1);
                }
                if constexpr (!std::is_same_v<Grid, SettlementGrid>)
                {
                    continue;
                }
                bool coast = false, sandEdge = false, shallowEdge = false;
                const WorldTile* landTile = isLand ? tileAt(x, y) : nullptr;
                bool coastSand = sand(x, y);
                const WorldTile* grassTile = nullptr;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const auto* t = tileAt(x + dx, y + dy);
                        coast |= land(x + dx, y + dy) != isLand;
                        sandEdge |= sand(x + dx, y + dy) != sand(x, y) &&
                                    land(x + dx, y + dy);
                        shallowEdge |=
                            kind(x + dx, y + dy) == CityTileType::ShallowWater;
                        if (!landTile && t->terrain != TerrainType::Water)
                        {
                            landTile = t;
                            coastSand = sand(x + dx, y + dy);
                        }
                        if (t->terrain == TerrainType::Land &&
                            !sand(x + dx, y + dy))
                        {
                            grassTile = t;
                        }
                    }
                }
                if (!coast && !(sand(x, y) && sandEdge) &&
                    !(kind(x, y) == CityTileType::DeepWater && shallowEdge))
                {
                    continue;
                }
                const auto* landArt = sprites.find(
                    coastSand  ? "terrain.beach"
                    : landTile ? terrainArtId(*landTile, sprites).c_str()
                               : "terrain.plain"
                );
                const auto* grassArt =
                    isLand && !sand(x, y) ? landArt
                    : grassTile
                        ? sprites.find(terrainArtId(*grassTile, sprites))
                        : landArt;
                const int n = 16;
                for (int row = 0; row < n; ++row)
                {
                    for (int col = 0; col < n;)
                    {
                        const auto material = [&](int c)
                        {
                            const double xx = x + (c + .5) / n,
                                         yy = y + (row + .5) / n;
                            const auto key =
                                (std::uint64_t(std::uint32_t(x)) << 32) |
                                std::uint32_t(y);
                            auto& fields = coastFields_[key];
                            const auto pixel = (n == 16 ? 64 : 0) + row * n + c;
                            if (!fields.ready[pixel])
                            {
                                const auto sample = coastSample(xx, yy, false);
                                fields.values[pixel] = {
                                    surfaceField(sample.x, sample.y, land),
                                    surfaceField(sample.x, sample.y, sand),
                                    surfaceField(
                                        xx,
                                        yy,
                                        [&](int a, int b)
                                        {
                                            return kind(a, b) ==
                                                   CityTileType::ShallowWater;
                                        }
                                    )
                                };
                                fields.ready.set(pixel);
                            }
                            const auto& field = fields.values[pixel];
                            const double landField = field[0];
                            if (coast && landField < .5)
                            {
                                if (landField > .46)
                                {
                                    return 1; // narrow wet shoreline
                                }
                                if (tp >= AnimationDetailPixels &&
                                    std::abs(
                                        landField -
                                        (.29 + .075 * std::sin(
                                                          sprites.time() * 1.4 +
                                                          xx * .8 + yy * .7
                                                      ))
                                    ) < .035 &&
                                    std::sin(
                                        xx * 3.1 - yy * 2.3 +
                                        sprites.time() * .3
                                    ) > -.25)
                                {
                                    return 2;
                                }
                                return 0;
                            }
                            if (!isLand && !coast)
                            {
                                const double shallow = field[2];
                                return shallow > .16 ? 5 : 0;
                            }
                            return field[1] > .47 ? 3 : 4;
                        };
                        const int m = material(col), first = col++;
                        while (col < n && material(col) == m)
                        {
                            ++col;
                        }
                        const double u = double(first) / n, v = double(row) / n,
                                     w = double(col - first) / n, h = 1. / n;
                        if (m == 0 || m == 5)
                        {
                            paint(
                                m == 5 ? sprites.find("terrain.shallow")
                                       : water,
                                x,
                                y,
                                u,
                                v,
                                w,
                                h
                            );
                        }
                        else if (m >= 3)
                        {
                            paint(
                                m == 3 ? sprites.find("terrain.beach")
                                       : grassArt,
                                x,
                                y,
                                u,
                                v,
                                w,
                                h
                            );
                        }
                        else
                        {
                            fillRectangle(
                                float(ox + (x + u) * tp),
                                float(oy + (y + v) * tp),
                                float(w * tp),
                                float(h * tp),
                                m == 2      ? RenderColor{175, 201, 214, 255}
                                : coastSand ? RenderColor{169, 148, 120, 255}
                                            : RenderColor{79, 140, 122, 255}
                            );
                        }
                    }
                }
            }
        }
        const double coastOpacity = detailBlend(tp, 8, 16);
        for (auto& draw : draws)
        {
            draw.opacity = std::uint8_t(draw.opacity * coastOpacity);
            draw.fill.alpha = std::uint8_t(draw.fill.alpha * coastOpacity);
        }
        renderer.drawTextureItems(draws);
    }
    void WorldGridRenderer::renderOverview(
        Renderer& renderer,
        float x,
        float y,
        float width,
        float height
    ) const
    {
        if (cachedTerrainTexture_ && width > 0.0F && height > 0.0F)
        {
            renderer.drawTexture(
                *cachedTerrainTexture_,
                0.0F,
                0.0F,
                static_cast<float>(cachedTerrainTexture_->width()),
                static_cast<float>(cachedTerrainTexture_->height()),
                x,
                y,
                width,
                height
            );
        }
    }


    template<typename Grid>
    void WorldGridRenderer::renderGrid(
        Renderer& renderer,
        const Grid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary* sprites
    ) const
    {
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());

        if (tilePixels <= 0.0)
        {
            return;
        }

        std::vector<std::shared_ptr<Texture>> sources;
        std::vector<double> dimensions;
        bool hasTerrainArt = false;
        for (const char* baseId : terrainIds)
        {
            const std::string id =
                std::is_same_v<Grid, WorldGrid> &&
                        std::string(baseId) != "terrain.mountain"
                    ? "world." + std::string(baseId)
                    : std::string(baseId);
            for (int variant = 0; variant <= 4; ++variant)
            {
                const auto* sprite =
                    sprites ? sprites->find(
                                  variant ? std::string(id) + "." +
                                                std::to_string(variant)
                                          : id
                              )
                            : nullptr;
                sources.push_back(sprite ? sprite->texture : nullptr);
                dimensions.push_back(sprite ? sprite->width : 0);
                dimensions.push_back(sprite ? sprite->height : 0);
                hasTerrainArt |= sprite != nullptr;
            }
        }
        if constexpr (std::is_same_v<Grid, WorldGrid>)
        {
            for (const auto* id :
                 {"world.relief.hill.1",
                  "world.relief.hill.2",
                  "world.relief.ridge.1",
                  "world.relief.ridge.2"})
            {
                const auto* art = sprites ? sprites->find(id) : nullptr;
                sources.push_back(art ? art->texture : nullptr);
            }
        }
        if (cachedGrid_ != &grid || sources != terrainSources_ ||
            dimensions != terrainDimensions_)
        {
            cachedGrid_ = &grid;
            cachedTerrainTexture_.reset();
            cacheBuildAttempted_ = false;
            terrainChunks_.clear();
            coastPaint_.clear();
            coastFields_.clear();
            terrainBytes_ = 0;
            terrainSources_ = std::move(sources);
            terrainDimensions_ = std::move(dimensions);
        }

        if (!cacheBuildAttempted_)
        {
            cacheBuildAttempted_ = true;
            if (overviewCancelled_)
            {
                overviewCancelled_->store(true);
            }
            overviewCancelled_ = std::make_shared<std::atomic_bool>(false);
            overviewReady_ = {};
            overviewUpload_.reset();
            overviewUploadRow_ = 0;
            std::unordered_map<const SceneSprite*, SceneSprite> cpuArt;
            std::vector<const SceneSprite*> materials(grid.tileCount());
            std::vector<RenderColor> pixels(grid.tileCount());
            // Only 81 city terrain/biome/temperature combinations exist. The
            // overview used to build and hash the same asset strings once per
            // map tile on the first city frame.
            std::array<const SceneSprite*, 81> resolved{};
            std::array<bool, 81> resolvedOnce{};
            const auto resolve =
                [&](const WorldTile& tile) -> const SceneSprite*
            {
                if constexpr (std::is_same_v<Grid, SettlementGrid>)
                {
                    const double t = tile.temperature.value();
                    const int climate = t > .68 ? 2 : t > 0 && t < .32 ? 1 : 0;
                    const int key =
                        int(tile.terrain) * 27 + int(tile.biome) * 3 + climate;
                    if (!resolvedOnce[key])
                    {
                        resolved[key] =
                            sprites->find(terrainArtId(tile, *sprites, false));
                        resolvedOnce[key] = true;
                    }
                    return resolved[key];
                }
                else
                {
                    return sprites->find(terrainArtId(tile, *sprites, true));
                }
            };


            for (std::int32_t y = 0; y < grid.height(); ++y)
            {
                for (std::int32_t x = 0; x < grid.width(); ++x)
                {
                    const auto& tile = *grid.tile({x, y});
                    auto color = hasTerrainArt ? artOverviewColor(tile)
                                               : tileColor(tile);
                    if (hasTerrainArt && sprites)
                    {
                        auto index = terrainIndex(tile);
                        if constexpr (std::is_same_v<Grid, SettlementGrid>)
                        {
                            if (grid.cityTileType({x, y}) ==
                                CityTileType::Beach)
                            {
                                index = 9;
                            }
                            else if (
                                grid.cityTileType({x, y}) ==
                                CityTileType::ShallowWater
                            )
                            {
                                index = 8;
                            }
                        }
                        if (const auto* art =
                                index < 8 ? resolve(tile)
                                          : sprites->find(terrainIds[index]);
                            art && art->overviewColor.alpha == 255)
                        {
                            color = art->overviewColor;
                            if (art->materialPixels && art->materialWidth >= 16)
                            {
                                auto [entry, inserted] =
                                    cpuArt.try_emplace(art, *art);
                                if (inserted)
                                {
                                    entry->second.texture.reset();
                                    entry->second.shadow.reset();
                                }
                                materials[std::size_t(y) * grid.width() + x] =
                                    &entry->second;
                            }
                        }
                    }
                    if (hasTerrainArt && tile.terrain == TerrainType::Water)
                    {
                        color = shoreWater(grid, x, y)
                                    ? RenderColor{0x54, 0x8A, 0xC4, 255}
                                    : RenderColor{0x3F, 0x5F, 0x9A, 255};
                    }
                    if (hasTerrainArt && tile.terrain == TerrainType::Mountain)
                    {
                        color = mountainInterior(grid, x, y)
                                    ? RenderColor{0x08, 0x0F, 0x1B, 255}
                                    : RenderColor{0x39, 0x46, 0x58, 255};
                        if constexpr (std::is_same_v<Grid, WorldGrid>)
                        {
                            color = {78, 59, 57, 255};
                        }
                    }
                    pixels
                        [static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(grid.width()) +
                         static_cast<std::size_t>(x)] = color;
                }
            }

            // A cheap authored fallback is visible immediately. Contour and
            // material sampling runs off the render thread, never at a zoom
            // boundary, and uploads in small strips when ready.
            cachedTerrainTexture_ = renderer.createTextureFromPixels(
                grid.width(),
                grid.height(),
                pixels
            );
            overviewPending_ = std::async(
                std::launch::async,
                [grid,
                 pixels = std::move(pixels),
                 cpuArt = std::move(cpuArt),
                 materials = std::move(materials),
                 cancelled = overviewCancelled_]()
                {
                    // Four samples per tile retain curved material contours at
                    // distant zoom, without traversing coastline geometry on
                    // camera frames.
                    constexpr int density = 4;
                    const int w = grid.width(), h = grid.height();
                    std::vector<RenderColor> smooth(
                        std::size_t(w * density) * h * density
                    );
                    for (int py = 0; py < h * density; ++py)
                    {
                        if (cancelled->load())
                        {
                            return OverviewData{};
                        }
                        for (int px = 0; px < w * density; ++px)
                        {
                            const auto sample = coastSample(
                                (px + .5) / density,
                                (py + .5) / density,
                                std::is_same_v<Grid, WorldGrid>
                            );
                            const double xx = sample.x - .5, yy = sample.y - .5;
                            const int ix = int(std::floor(xx)),
                                      iy = int(std::floor(yy));
                            double u = xx - ix, v = yy - iy;
                            u = u * u * (3 - 2 * u);
                            v = v * v * (3 - 2 * v);
                            double weights[3] = {}, r[3] = {}, g[3] = {},
                                   b[3] = {};
                            for (int j = 0; j < 2; ++j)
                            {
                                for (int i = 0; i < 2; ++i)
                                {
                                    const int x = std::clamp(ix + i, 0, w - 1),
                                              y = std::clamp(iy + j, 0, h - 1);
                                    const auto* tile = grid.tile({x, y});
                                    const int k =
                                        tile->terrain == TerrainType::Water ? 0
                                        : tile->terrain == TerrainType::Mountain
                                            ? 2
                                            : 1;
                                    const double weight =
                                        (i ? u : 1 - u) * (j ? v : 1 - v);
                                    auto c = pixels[std::size_t(y) * w + x];
                                    if constexpr (
                                        std::is_same_v<Grid, WorldGrid>
                                    )
                                    {
                                        if (k == 0)
                                        {
                                            c = {0x20, 0x2C, 0x43, 255};
                                        }
                                    }
                                    weights[k] += weight;
                                    r[k] += c.red * weight;
                                    g[k] += c.green * weight;
                                    b[k] += c.blue * weight;
                                }
                            }
                            const int k = weights[0] > .5   ? 0
                                          : weights[2] > .5 ? 2
                                          : weights[1] > 0  ? 1
                                                            : 2;
                            const double sum = std::max(weights[k], .0001);
                            smooth[std::size_t(py) * w * density + px] = {
                                std::uint8_t(r[k] / sum),
                                std::uint8_t(g[k] / sum),
                                std::uint8_t(b[k] / sum),
                                255
                            };
                            if (k == 1)
                            {
                                double accumulated = 0;
                                const double choice = landscapeField(
                                                          sample.x * 2.1,
                                                          sample.y * 2.1,
                                                          193
                                                      ) *
                                                      sum;
                                bool chosen = false;
                                for (int j = 0; j < 2; ++j)
                                {
                                    for (int i = 0; i < 2; ++i)
                                    {
                                        int x = std::clamp(ix + i, 0, w - 1),
                                            y = std::clamp(iy + j, 0, h - 1);
                                        const auto* t = grid.tile({x, y});
                                        if (t->terrain != TerrainType::Land)
                                        {
                                            continue;
                                        }
                                        accumulated +=
                                            (i ? u : 1 - u) * (j ? v : 1 - v);
                                        const auto* material =
                                            materials[std::size_t(y) * w + x];
                                        if (!chosen && choice < accumulated &&
                                            material)
                                        {
                                            smooth
                                                [std::size_t(py) * w * density +
                                                 px] =
                                                    landscapePaint(
                                                        *material,
                                                        (px + .5) / density,
                                                        (py + .5) / density,
                                                        std::is_same_v<
                                                            Grid,
                                                            WorldGrid>
                                                    );
                                            chosen = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return OverviewData{
                        w * density,
                        h * density,
                        std::move(smooth)
                    };
                }
            );
        }
        if (overviewPending_.valid() &&
            overviewPending_.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready)
        {
            overviewReady_ = overviewPending_.get();
        }
        if (overviewReady_.width)
        {
            if (!overviewUpload_)
            {
                overviewUpload_ = renderer.createEmptyTexture(
                    overviewReady_.width,
                    overviewReady_.height
                );
            }
            const int rows =
                std::min(32, overviewReady_.height - overviewUploadRow_);
            renderer.updateTextureRegion(
                *overviewUpload_,
                0,
                overviewUploadRow_,
                overviewReady_.width,
                rows,
                std::span(overviewReady_.pixels)
                    .subspan(
                        std::size_t(overviewUploadRow_) * overviewReady_.width,
                        std::size_t(rows) * overviewReady_.width
                    )
            );
            overviewUploadRow_ += rows;
            if (overviewUploadRow_ == overviewReady_.height)
            {
                cachedTerrainTexture_ = std::move(overviewUpload_);
                overviewReady_ = {};
            }
        }

        if (!cachedTerrainTexture_)
        {
            // Never fall back to per-tile drawing. A failed cache should
            // remain visible as a rendering failure, not freeze the game.
            return;
        }

        const double viewportWidth =
            static_cast<double>(renderer.outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer.outputHeight());

        renderer.drawTexture(
            *cachedTerrainTexture_,
            0.0F,
            0.0F,
            static_cast<float>(cachedTerrainTexture_->width()),
            static_cast<float>(cachedTerrainTexture_->height()),
            static_cast<float>(
                viewportWidth * 0.5 - camera.tileX() * tilePixels
            ),
            static_cast<float>(
                viewportHeight * 0.5 - camera.tileY() * tilePixels
            ),
            static_cast<float>(static_cast<double>(grid.width()) * tilePixels),
            static_cast<float>(static_cast<double>(grid.height()) * tilePixels)
        );
        if (hasTerrainArt && sprites)
        {
            renderSpriteTerrain(renderer, grid, camera, metrics, *sprites);
        }
    }
    template<typename Grid>
    void WorldGridRenderer::renderSpriteTerrain(
        Renderer& renderer,
        const Grid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SceneSpriteLibrary& sprites,
        const std::function<void(const Texture&, int, int)>& project
    ) const
    {
        const double displayTilePixels =
            metrics.scaledTilePixels(camera.zoom());
        const bool drawDetail = displayTilePixels > 8 || bool(project);
        // Prepare canonical ground while zoomed out, within the same per-frame
        // budget. Entering normal city zoom should not discover every ground
        // chunk cold and expose a central patch of detail over the overview.
        const double tilePixels = drawDetail ? displayTilePixels : 12.;
        // A 4-tile work unit keeps CPU composition bounded even in /Od
        // debugger builds. Resolution and the resulting artwork are unchanged.
        constexpr int chunkSide = 4;
        int resolution = 16;
        constexpr std::size_t cacheBytes = 64 * 1024 * 1024;
        if (terrainPixelsPerTile_ != resolution)
        {
            terrainPixelsPerTile_ = resolution;
        }
        const int textureSide = chunkSide * resolution;
        const std::size_t cacheLimit = std::clamp(
            cacheBytes / (textureSide * textureSide * 4),
            std::size_t(16),
            std::size_t(4096)
        );
        const double originX =
            renderer.outputWidth() * .5 - camera.tileX() * tilePixels;
        const double originY =
            renderer.outputHeight() * .5 - camera.tileY() * tilePixels;
        const int columns = (grid.width() + chunkSide - 1) / chunkSide;
        const int rows = (grid.height() + chunkSide - 1) / chunkSide;
        const int firstX = std::clamp(
            int(std::floor(-originX / tilePixels / chunkSide)),
            0,
            columns
        );
        const int firstY = std::clamp(
            int(std::floor(-originY / tilePixels / chunkSide)),
            0,
            rows
        );
        const int lastX = std::clamp(
            int(std::ceil(
                (renderer.outputWidth() - originX) / tilePixels / chunkSide
            )),
            0,
            columns
        );
        const int lastY = std::clamp(
            int(std::ceil(
                (renderer.outputHeight() - originY) / tilePixels / chunkSide
            )),
            0,
            rows
        );
        struct VisibleChunk
        {
            int x, y;
            double distance;
        };
        std::vector<VisibleChunk> visible;
        for (int cy = firstY; cy < lastY; ++cy)
        {
            for (int cx = firstX; cx < lastX; ++cx)
            {
                const double dx = (cx + .5) * chunkSide - camera.tileX();
                const double dy = (cy + .5) * chunkSide - camera.tileY();
                visible.push_back({cx, cy, dx * dx + dy * dy});
            }
        }
        std::sort(
            visible.begin(),
            visible.end(),
            [](const auto& a, const auto& b) { return a.distance < b.distance; }
        );
        ++terrainFrame_;
        // Keep a stable nearest set when the viewport exceeds the cache.
        // The overview still covers the whole map outside this detail budget.
        visible.resize(std::min(visible.size(), cacheLimit));
        int rebuildBudget = 2;
        const auto deadline = SDL_GetTicksNS() + 2000000;
        std::vector<TextureDrawItem> items;
        items.reserve(chunkSide * chunkSide);
        for (const auto& cell : visible)
        {
            const auto key = (std::uint64_t(std::uint32_t(cell.x)) << 32) |
                             std::uint32_t(cell.y);
            auto found = terrainChunks_.find(key);
            if (found == terrainChunks_.end() ||
                found->second.resolution != resolution)
            {
                // Bound CPU composition as well as texture uploads. The
                // overview covers newly exposed areas while detail arrives.
                if (rebuildBudget == 0 ||
                    (rebuildBudget < 2 && SDL_GetTicksNS() >= deadline))
                {
                    continue;
                }
                if (found == terrainChunks_.end() &&
                    terrainChunks_.size() >= cacheLimit)
                {
                    const auto oldest = std::min_element(
                        terrainChunks_.begin(),
                        terrainChunks_.end(),
                        [](const auto& a, const auto& b)
                        { return a.second.lastUsed < b.second.lastUsed; }
                    );
                    if (oldest->second.texture)
                    {
                        terrainBytes_ -=
                            std::size_t(oldest->second.texture->width()) *
                            oldest->second.texture->height() * 4;
                    }
                    terrainChunks_.erase(oldest);
                }
                items.clear();
                for (int localY = 0; localY < chunkSide; ++localY)
                {
                    const int y = cell.y * chunkSide + localY;
                    if (y >= grid.height())
                    {
                        break;
                    }
                    for (int localX = 0; localX < chunkSide; ++localX)
                    {
                        const int x = cell.x * chunkSide + localX;
                        if (x >= grid.width())
                        {
                            break;
                        }
                        const auto& tile = *grid.tile({x, y});
                        std::string id = terrainArtId(
                            tile,
                            sprites,
                            std::is_same_v<Grid, WorldGrid>
                        );
                        if constexpr (std::is_same_v<Grid, SettlementGrid>)
                        {
                            if (grid.cityTileType({x, y}) ==
                                CityTileType::Beach)
                            {
                                id = "terrain.beach";
                            }
                            else if (
                                grid.cityTileType({x, y}) ==
                                CityTileType::ShallowWater
                            )
                            {
                                id = "terrain.shallow";
                            }
                        }
                        if (shoreWater(grid, x, y))
                        {
                            id = std::is_same_v<Grid, WorldGrid>
                                     ? "world.terrain.shallow"
                                     : "terrain.shallow";
                        }
                        const auto* sprite = sprites.find(id);
                        if (sprite && sprite->materialPixels &&
                            sprite->materialWidth >= 16)
                        {
                            constexpr bool world =
                                std::is_same_v<Grid, WorldGrid>;
                            const SceneSprite* materials[9] = {};
                            double exposures[9] = {};
                            if (true)
                            {
                                for (int j = 0; j < 3; ++j)
                                {
                                    for (int i = 0; i < 3; ++i)
                                    {
                                        const auto& adjacent = *grid.tile(
                                            {std::clamp(
                                                 x + i - 1,
                                                 0,
                                                 grid.width() - 1
                                             ),
                                             std::clamp(
                                                 y + j - 1,
                                                 0,
                                                 grid.height() - 1
                                             )}
                                        );
                                        if (adjacent.terrain !=
                                            TerrainType::Water)
                                        {
                                            materials[j * 3 + i] =
                                                sprites.find(terrainArtId(
                                                    adjacent,
                                                    sprites,
                                                    world
                                                ));
                                        }
                                        exposures[j * 3 + i] =
                                            adjacent.terrain ==
                                                    TerrainType::Mountain
                                                ? .88
                                            : adjacent.biome == BiomeType::Hills
                                                ? .42
                                                : 0;
                                    }
                                }
                            }
                            const bool buriedMountain =
                                mountainInterior(grid, x, y);
                            const bool uniformMaterial = std::all_of(
                                std::begin(materials),
                                std::end(materials),
                                [&](const SceneSprite* material)
                                { return !material || material == sprite; }
                            );
                            const auto paint = [&](int px, int py)
                            {
                                const double xx = x + (px + .5) / resolution,
                                             yy = y + (py + .5) / resolution;
                                auto color =
                                    landscapePaint(*sprite, xx, yy, world);
                                if (world || !uniformMaterial)
                                {
                                    if (tile.terrain != TerrainType::Water)
                                    {
                                        const double sx = xx - .5, sy = yy - .5;
                                        const int ix = int(std::floor(sx)),
                                                  iy = int(std::floor(sy));
                                        double u = sx - ix, v = sy - iy;
                                        u = u * u * (3 - 2 * u);
                                        v = v * v * (3 - 2 * v);
                                        const double weights[4] = {
                                            (1 - u) * (1 - v),
                                            u * (1 - v),
                                            (1 - u) * v,
                                            u * v
                                        };
                                        double exposure = 0, cumulative = 0;
                                        const double choose = landscapeField(
                                            xx * 2.1,
                                            yy * 2.1,
                                            193
                                        );
                                        bool chosen = false;
                                        for (int n = 0; n < 4; ++n)
                                        {
                                            const int cell =
                                                (iy + n / 2 - y + 1) * 3 + ix +
                                                n % 2 - x + 1;
                                            exposure +=
                                                weights[n] * exposures[cell];
                                            cumulative += weights[n];
                                            if (!chosen &&
                                                choose < cumulative &&
                                                materials[cell])
                                            {
                                                chosen = true;
                                                if (materials[cell] != sprite)
                                                {
                                                    color = landscapePaint(
                                                        *materials[cell],
                                                        xx,
                                                        yy,
                                                        world
                                                    );
                                                }
                                            }
                                        }
                                        if constexpr (world)
                                        {
                                            const double patch =
                                                .55 * landscapeField(
                                                          xx * .38,
                                                          yy * .38,
                                                          931
                                                      ) +
                                                .45 * landscapeField(
                                                          xx * 5,
                                                          yy * 5,
                                                          981
                                                      );
                                            if (world && patch < exposure)
                                            {
                                                const double grain =
                                                    landscapeField(
                                                        xx * 4,
                                                        yy * 4,
                                                        713
                                                    );
                                                color =
                                                    grain < .24
                                                        ? RenderColor{116, 81, 63, 255}
                                                    : grain > .79
                                                        ? RenderColor{113, 109, 112, 255}
                                                        : RenderColor{
                                                              78,
                                                              59,
                                                              57,
                                                              255
                                                          };
                                            }
                                        }
                                    }
                                }
                                if (buriedMountain)
                                {
                                    color = {8, 15, 27, 255};
                                }
                                return color;
                            };
                            for (int py = 0; py < resolution; ++py)
                            {
                                std::array<RenderColor, 16> rowColors;
                                for (int px = 0; px < resolution; ++px)
                                {
                                    rowColors[px] = paint(px, py);
                                }
                                for (int px = 0; px < resolution;)
                                {
                                    const int first = px++;
                                    const auto color = rowColors[first];
                                    while (px < resolution &&
                                           samePaint(color, rowColors[px]))
                                    {
                                        ++px;
                                    }
                                    items.push_back(
                                        {nullptr,
                                         {},
                                         {float(localX * resolution + first),
                                          float(localY * resolution + py),
                                          float(px - first),
                                          1},
                                         color,
                                         255}
                                    );
                                }
                            }
                        }
                        else
                        {
                            TextureDrawItem item;
                            item.destination = {
                                float(localX * resolution),
                                float(localY * resolution),
                                float(resolution),
                                float(resolution)
                            };
                            if (sprite)
                            {
                                const int w = std::max(1, int(sprite->width)),
                                          h = std::max(1, int(sprite->height));
                                const float
                                    sw = float(sprite->texture->width()) /
                                         sprite->frames / w,
                                    sh = float(sprite->texture->height()) / h;
                                item.texture = sprite->texture.get();
                                item.source = {
                                    float(x % w) * sw,
                                    float(y % h) * sh,
                                    sw,
                                    sh
                                };
                            }
                            else
                            {
                                item.fill = tileColor(tile);
                            }
                            items.push_back(item);
                        }
                    }
                }
                if constexpr (std::is_same_v<Grid, WorldGrid>)
                {
                    TerrainComposer target{items, textureSide};
                    Camera2D local(
                        (cell.x + .5) * chunkSide,
                        (cell.y + .5) * chunkSide
                    );
                    TileRenderMetrics localMetrics{double(resolution)};
                    worldTerrainEdges(
                        target,
                        grid,
                        local,
                        localMetrics,
                        sprites
                    );
                    mountainRanges(
                        target,
                        grid,
                        local,
                        localMetrics,
                        sprites,
                        !project
                    );
                }
                TerrainChunk chunk;
                chunk.resolution = resolution;
                chunk.commands = items;
                found = terrainChunks_.insert_or_assign(key, std::move(chunk))
                            .first;
            }
            if (!found->second.texture && rebuildBudget > 0)
            {
                --rebuildBudget;
                found->second.texture = renderer.createTextureFromDrawItems(
                    textureSide,
                    textureSide,
                    found->second.commands
                );
                if (found->second.texture)
                {
                    terrainBytes_ += std::size_t(textureSide) * textureSide * 4;
                    found->second.commands.clear();
                    found->second.commands.shrink_to_fit();
                }
            }

            found->second.lastUsed = terrainFrame_;
            if (!drawDetail)
            {
                continue;
            }
            // Retained textures at different resolutions share one byte cap.
            while (terrainBytes_ > cacheBytes && terrainChunks_.size() > 1)
            {
                auto victim = terrainChunks_.end();
                for (auto it = terrainChunks_.begin();
                     it != terrainChunks_.end();
                     ++it)
                {
                    if (it != found &&
                        (victim == terrainChunks_.end() ||
                         it->second.lastUsed < victim->second.lastUsed))
                    {
                        victim = it;
                    }
                }
                if (victim == terrainChunks_.end())
                {
                    break;
                }
                if (victim->second.texture)
                {
                    terrainBytes_ -=
                        std::size_t(victim->second.texture->width()) *
                        victim->second.texture->height() * 4;
                }
                terrainChunks_.erase(victim);
            }
            if (found->second.texture)
            {
                if (project)
                {
                    project(
                        *found->second.texture,
                        cell.x * chunkSide,
                        cell.y * chunkSide
                    );
                    continue;
                }
                renderer.drawTexture(
                    *found->second.texture,
                    0,
                    0,
                    float(found->second.texture->width()),
                    float(found->second.texture->height()),
                    float(originX + cell.x * chunkSide * tilePixels),
                    float(originY + cell.y * chunkSide * tilePixels),
                    float(chunkSide * tilePixels),
                    float(chunkSide * tilePixels),
                    std::uint8_t(255 * detailBlend(displayTilePixels, 8, 16))
                );
            }
            else
            {
                // The uncached draw is identical to its future texture. Cache
                // completion never changes visibility, color, or opacity.
                const float scale = float(tilePixels / resolution);
                for (const auto& item : found->second.commands)
                {
                    const auto& d = item.destination;
                    const float x =
                        float(originX + cell.x * chunkSide * tilePixels) +
                        d.x * scale;
                    const float y =
                        float(originY + cell.y * chunkSide * tilePixels) +
                        d.y * scale;
                    if (item.texture)
                    {
                        renderer.drawTexture(
                            *item.texture,
                            item.source.x,
                            item.source.y,
                            item.source.width,
                            item.source.height,
                            x,
                            y,
                            d.width * scale,
                            d.height * scale,
                            std::uint8_t(
                                item.opacity *
                                detailBlend(displayTilePixels, 8, 16)
                            )
                        );
                    }
                    else
                    {
                        renderer.fillRectangle(
                            x,
                            y,
                            d.width * scale,
                            d.height * scale,
                            RenderColor{
                                item.fill.red,
                                item.fill.green,
                                item.fill.blue,
                                std::uint8_t(
                                    item.fill.alpha *
                                    detailBlend(displayTilePixels, 8, 16)
                                )
                            }
                        );
                    }
                }
            }
        }
    }
} // namespace Paladin
