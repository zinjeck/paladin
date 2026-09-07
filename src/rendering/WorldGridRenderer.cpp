#include "rendering/WorldGridRenderer.h"
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/SceneDetail.h"

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
            if (tile.terrain == TerrainType::Water)
            {
                return 6;
            }
            if (tile.terrain == TerrainType::Mountain)
            {
                return 7;
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
            const SceneSpriteLibrary& sprites
        )
        {
            std::string id = terrainIds[terrainIndex(tile)];
            if (tile.terrain == TerrainType::Land &&
                (tile.biome == BiomeType::Plain ||
                 tile.biome == BiomeType::Forest ||
                 tile.biome == BiomeType::Jungle ||
                 tile.biome == BiomeType::Taiga))
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

            case BiomeType::Forest:
                return {26, 107, 41, 255};

            case BiomeType::Jungle:
                return {5, 92, 23, 255};

            case BiomeType::Desert:
                return {219, 184, 92, 255};

            case BiomeType::Tundra:
                return {163, 184, 173, 255};

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
        void mountainRanges(
            Renderer& renderer,
            const WorldGrid& grid,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SceneSpriteLibrary& sprites
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
            for (int row = y0 / 2; row <= (y1 + 1) / 2; ++row)
            {
                for (int col = x0 / 3; col <= (x1 + 2) / 3; ++col)
                {
                    const int x = col * 3 + (row % 2), y = row * 2;
                    bool covered = true;
                    for (int yy = y - 2; yy <= y + 1 && covered; ++yy)
                    {
                        for (int xx = x - 1; xx <= x + 2; ++xx)
                        {
                            const auto* tile = grid.tile({xx, yy});
                            if (!tile || tile->terrain != TerrainType::Mountain)
                            {
                                covered = false;
                                break;
                            }
                        }
                    }
                    if (!covered)
                    {
                        continue;
                    }
                    const std::uint64_t seed =
                        std::uint64_t(col) * 0x9E3779B185EBCA87ull ^
                        std::uint64_t(row) * 0xC2B2AE3D27D4EB4Full;
                    const auto* art = sprites.find(
                        (seed ^ (seed >> 17)) % 2 ? "mountain.peak.a"
                                                  : "mountain.peak.b"
                    );
                    if (!art)
                    {
                        continue;
                    }
                    const double width = 3.5 + ((seed >> 8) % 4) * .1,
                                 height = 3.6 + ((seed >> 12) % 3) * .1;
                    const auto frame = sprites.frame(*art, false);
                    renderer.drawTexture(
                        *art->texture,
                        frame.x,
                        frame.y,
                        frame.width,
                        frame.height,
                        float(ox + (x + .5 - width * .5) * tp),
                        float(oy + (y + 1.8 - height) * tp),
                        float(width * tp),
                        float(height * tp),
                        std::uint8_t(255 * detailBlend(tp, 12, 24))
                    );
                }
            }
        }
    } // namespace


    WorldGridRenderer::WorldGridRenderer() = default;


    WorldGridRenderer::~WorldGridRenderer() = default;


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
            renderCoast(renderer, grid, camera, metrics, *sprites);
            mountainRanges(renderer, grid, camera, metrics, *sprites);
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
        if (tp < AnimationDetailPixels || !sprites.find("terrain.shallow") ||
            !sprites.find("terrain.beach"))
        {
            return;
        }
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
            auto f = sprites.frame(*s);
            const int mw = std::max(1, int(s->width)),
                      mh = std::max(1, int(s->height));
            if (s->frames > 1 && s->fps > 0)
            {
                f.x = float(
                          int(sprites.time() * s->fps +
                              (x / mw * 13 + y / mh * 7) % s->frames) %
                          s->frames
                      ) *
                      f.width;
            }
            renderer.drawTexture(
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
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
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
                const int n = tp >= 12 ? 16 : 8;
                for (int row = 0; row < n; ++row)
                {
                    for (int col = 0; col < n;)
                    {
                        const auto material = [&](int c)
                        {
                            const double xx = x + (c + .5) / n,
                                         yy = y + (row + .5) / n;
                            const double grain =
                                .016 * std::sin(xx * 13 + yy * 7);
                            const double landField =
                                surfaceField(xx, yy, land) + grain;
                            if (coast && landField < .5)
                            {
                                if (landField > .40)
                                {
                                    return 1; // narrow wet shoreline
                                }
                                if (landField > .31 &&
                                    ((int(xx * 8 + yy * 5) +
                                      int(sprites.time() * 2)) %
                                     9) < 3)
                                {
                                    return 2;
                                }
                                return 0;
                            }
                            if (!isLand && !coast)
                            {
                                const double shallow = surfaceField(
                                    xx,
                                    yy,
                                    [&](int a, int b)
                                    {
                                        return kind(a, b) ==
                                               CityTileType::ShallowWater;
                                    }
                                );
                                return shallow > .16 ? 5 : 0;
                            }
                            return surfaceField(xx, yy, sand) + grain > .47 ? 3
                                                                            : 4;
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
                            renderer.fillRectangle(
                                float(ox + (x + u) * tp),
                                float(oy + (y + v) * tp),
                                float(w * tp),
                                float(h * tp),
                                m == 2      ? RenderColor{175, 201, 214, 255}
                                : coastSand ? RenderColor{169, 148, 120, 255}
                                            : RenderColor{25, 62, 66, 255}
                            );
                        }
                    }
                }
            }
        }
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
        for (const char* id : terrainIds)
        {
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
        if (cachedGrid_ != &grid || sources != terrainSources_ ||
            dimensions != terrainDimensions_)
        {
            cachedGrid_ = &grid;
            cachedTerrainTexture_.reset();
            cacheBuildAttempted_ = false;
            terrainChunks_.clear();
            terrainBytes_ = 0;
            terrainSources_ = std::move(sources);
            terrainDimensions_ = std::move(dimensions);
        }

        if (!cacheBuildAttempted_)
        {
            cacheBuildAttempted_ = true;
            std::vector<RenderColor> pixels(grid.tileCount());

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
                        if (const auto* art = sprites->find(
                                index < 8 ? terrainArtId(tile, *sprites)
                                          : std::string(terrainIds[index])
                            );
                            art && art->overviewColor.alpha == 255)
                        {
                            color = art->overviewColor;
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
                    }
                    pixels
                        [static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(grid.width()) +
                         static_cast<std::size_t>(x)] = color;
                }
            }

            cachedTerrainTexture_ = renderer.createTextureFromPixels(
                grid.width(),
                grid.height(),
                pixels
            );
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
            static_cast<float>(grid.width()),
            static_cast<float>(grid.height()),
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
        const SceneSpriteLibrary& sprites
    ) const
    {
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        // The overview cache is the appropriate representation once several
        // authored pixels occupy the same display pixel. Never traverse the
        // entire world to draw tiny sprites at strategic zoom.
        if (tilePixels < 1.5)
        {
            return;
        }
        constexpr int chunkSide = 16;
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
            if (rebuildBudget > 0 && SDL_GetTicksNS() < deadline &&
                (found == terrainChunks_.end() ||
                 found->second.resolution != resolution))
            {
                --rebuildBudget;
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
                        std::string id = terrainArtId(tile, sprites);
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
                            id = "terrain.shallow";
                        }
                        const auto* sprite = sprites.find(id);
                        // A variant belongs to a whole module, so changing
                        // tile coordinates never splits an artist's 2x2 tile.
                        const int baseWidth =
                            sprite ? std::max(1, int(std::round(sprite->width)))
                                   : 1;
                        const int baseHeight =
                            sprite
                                ? std::max(1, int(std::round(sprite->height)))
                                : 1;
                        const auto stable = std::uint64_t(x / baseWidth) *
                                                0x9E3779B185EBCA87ull ^
                                            std::uint64_t(y / baseHeight) *
                                                0xC2B2AE3D27D4EB4Full;
                        if (const auto* variation = sprites.find(
                                id + "." +
                                std::to_string(
                                    1 + ((stable ^ (stable >> 17)) % 4)
                                )
                            ))
                        {
                            sprite = variation;
                        }
                        TextureDrawItem item;
                        item.destination = {
                            float(localX * resolution),
                            float(localY * resolution),
                            float(resolution),
                            float(resolution)
                        };
                        if (sprite)
                        {
                            const int width =
                                std::max(1, int(std::round(sprite->width)));
                            const int height =
                                std::max(1, int(std::round(sprite->height)));
                            const float sourceWidth =
                                float(sprite->texture->width()) /
                                std::max(1, sprite->frames) / width;
                            const float sourceHeight =
                                float(sprite->texture->height()) / height;
                            item.texture = sprite->texture.get();
                            item.source = {
                                float(x % width) * sourceWidth,
                                float(y % height) * sourceHeight,
                                sourceWidth,
                                sourceHeight
                            };
                        }
                        else
                        {
                            item.fill = tileColor(tile);
                        }
                        if (mountainInterior(grid, x, y))
                        {
                            item.texture = nullptr;
                            item.fill = {0x08, 0x0F, 0x1B, 255};
                        }
                        items.push_back(item);
                    }
                }
                TerrainChunk chunk;
                chunk.resolution = resolution;
                chunk.readyAt = SDL_GetTicksNS() / 1e9;
                chunk.texture = renderer.createTextureFromDrawItems(
                    textureSide,
                    textureSide,
                    items
                );
                if (chunk.texture)
                {
                    if (found != terrainChunks_.end() && found->second.texture)
                    {
                        terrainBytes_ -=
                            std::size_t(found->second.texture->width()) *
                            found->second.texture->height() * 4;
                    }
                    terrainBytes_ += std::size_t(textureSide) * textureSide * 4;
                    found =
                        terrainChunks_.insert_or_assign(key, std::move(chunk))
                            .first;
                }
            }
            if (found == terrainChunks_.end())
            {
                continue;
            }
            found->second.lastUsed = terrainFrame_;
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
                    std::uint8_t(
                        255 * detailBlend(tilePixels, 2, 10) *
                        detailBlend(
                            SDL_GetTicksNS() / 1e9 - found->second.readyAt,
                            0,
                            .18
                        )
                    )
                );
            }
        }
    }
} // namespace Paladin
