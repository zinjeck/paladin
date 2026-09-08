#include "rendering/SettlementNaturalFeatureRenderer.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneDetail.h"
#include "rendering/TileRenderMetrics.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    namespace
    {
        // Replaceable presentation mask, independent of feature identity and
        // logic.
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
        constexpr int side = 16,
                      block = SettlementNaturalFeatures::OverviewSide;
        constexpr int density = 16, textureSide = side * density;
        const int columns = (map.grid().width() + side - 1) / side,
                  rows = (map.grid().height() + side - 1) / side;
        const bool artwork = sprites && sprites->find("tree");
        if (sourceInstance_ != map.instanceId() || overviewArt_ != artwork)
        {
            sourceInstance_ = map.instanceId();
            overviewArt_ = artwork;
            cachedTextures_ = 0;
            chunks_.clear();
            chunks_.resize(std::size_t(columns) * rows);
            featureChunks_.clear();
            featureChunks_.resize(
                std::size_t((map.grid().width() + 31) / 32) *
                ((map.grid().height() + 31) / 32)
            );
            overviewTexture_.reset();
        }
        const auto blend = [](double a, double b, double x)
        {
            const auto t = std::clamp((x - a) / (b - a), 0., 1.);
            return t * t * (3 - 2 * t);
        };
        const double tp = metrics.scaledTilePixels(camera.zoom());
        const double ox = renderer.outputWidth() * .5 - camera.tileX() * tp,
                     oy = renderer.outputHeight() * .5 - camera.tileY() * tp;
        const int firstX =
                      std::clamp(int(std::floor(-ox / tp / side)), 0, columns),
                  firstY =
                      std::clamp(int(std::floor(-oy / tp / side)), 0, rows);
        const int lastX = std::clamp(
                      int(std::ceil((renderer.outputWidth() - ox) / tp / side)),
                      0,
                      columns
                  ),
                  lastY = std::clamp(
                      int(
                          std::ceil((renderer.outputHeight() - oy) / tp / side)
                      ),
                      0,
                      rows
                  );
        const int overviewW = (map.grid().width() + block - 1) / block,
                  overviewH = (map.grid().height() + block - 1) / block;
        const auto overviewPixel = [&](int x, int y)
        {
            const auto counts =
                map.naturalFeatures().overview({x * block, y * block});
            const bool tree = counts[0] >= counts[1];
            RenderColor c = tree ? RenderColor{0x23, 0x57, 0x47, 255}
                                 : RenderColor{0xA9, 0x94, 0x78, 255};
            c.alpha =
                std::uint8_t(std::min(235, int(counts[0] + counts[1]) * 48));
            return c;
        };
        if (!overviewTexture_)
        {
            std::vector<RenderColor> pixels(std::size_t(overviewW) * overviewH);
            for (int y = 0; y < overviewH; ++y)
            {
                for (int x = 0; x < overviewW; ++x)
                {
                    pixels[std::size_t(y) * overviewW + x] =
                        overviewPixel(x, y);
                }
            }
            overviewTexture_ =
                renderer.createTextureFromPixels(overviewW, overviewH, pixels);
            for (int y = 0; y < rows; ++y)
            {
                for (int x = 0; x < columns; ++x)
                {
                    chunks_[std::size_t(y) * columns + x].baseVersion =
                        map.naturalFeatures().chunkVersion(
                            x * side / 32,
                            y * side / 32
                        );
                }
            }
        }
        if (tp < 4)
        {
            // One density image at strategic zoom. Refresh a bounded number of
            // dirty blocks.
            for (std::size_t n = 0;
                 n < std::min(std::size_t(32), chunks_.size());
                 ++n)
            {
                const auto index = refreshCursor_++ % chunks_.size();
                auto& c = chunks_[index];
                const int cx = int(index % columns), cy = int(index / columns);
                const auto version = map.naturalFeatures().chunkVersion(
                    cx * side / 32,
                    cy * side / 32
                );
                if (c.baseVersion == version)
                {
                    continue;
                }
                const int x = cx * side / block, y = cy * side / block,
                          w = std::min(side / block, overviewW - x),
                          h = std::min(side / block, overviewH - y);
                std::vector<RenderColor> pixels(std::size_t(w) * h);
                for (int yy = 0; yy < h; ++yy)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        pixels[yy * w + xx] = overviewPixel(x + xx, y + yy);
                    }
                }
                renderer
                    .updateTextureRegion(*overviewTexture_, x, y, w, h, pixels);
                c.baseVersion = version;
            }
            renderer.drawTexture(
                *overviewTexture_,
                0,
                0,
                float(overviewW),
                float(overviewH),
                float(ox),
                float(oy),
                float(overviewW * block * tp),
                float(overviewH * block * tp)
            );
            return;
        }
        struct Visible
        {
            int x, y;
            double distance;
        };
        std::vector<Visible> visible;
        for (int y = firstY; y < lastY; ++y)
        {
            for (int x = firstX; x < lastX; ++x)
            {
                const double dx = x * side + side * .5 - camera.tileX(),
                             dy = y * side + side * .5 - camera.tileY();
                visible.push_back({x, y, dx * dx + dy * dy});
            }
        }
        std::sort(
            visible.begin(),
            visible.end(),
            [](const auto& a, const auto& b) { return a.distance < b.distance; }
        );
        // Amortize eviction; never sweep every map chunk on a camera frame.
        for (std::size_t n = 0; n < std::min(std::size_t(32), chunks_.size());
             ++n)
        {
            const auto index = refreshCursor_++ % chunks_.size();
            auto& c = chunks_[index];
            const int x = int(index % columns), y = int(index / columns);
            if (c.texture && (x < firstX - 2 || x >= lastX + 2 ||
                              y < firstY - 2 || y >= lastY + 2))
            {
                c.texture.reset();
                c.version = 0;
                --cachedTextures_;
            }
        }
        auto& textures = cachedTextures_;
        const auto deadline = SDL_GetTicksNS() + 2000000;
        int built = 0;
        const double detail = tp >= 40 ? 1.0 : 0.0;
        for (const auto& cell : visible)
        {
            auto& chunk = chunks_[std::size_t(cell.y) * columns + cell.x];
            const auto baseVersion = map.naturalFeatures().chunkVersion(
                cell.x * side / 32,
                cell.y * side / 32
            );
            std::uint64_t version = 1469598103934665603ull;
            // Crowns can cross a cache border. Include the neighboring
            // resource chunks so harvesting cannot leave a frozen overhang.
            for (int yy = std::max(0, cell.y * side - 2) / 32;
                 yy <=
                 std::min(map.grid().height() - 1, (cell.y + 1) * side + 2) /
                     32;
                 ++yy)
            {
                for (int xx = std::max(0, cell.x * side - 2) / 32;
                     xx <=
                     std::min(map.grid().width() - 1, (cell.x + 1) * side + 2) /
                         32;
                     ++xx)
                {
                    version =
                        (version ^ map.naturalFeatures().chunkVersion(xx, yy)) *
                        1099511628211ull;
                }
            }
            if (overviewTexture_ && chunk.baseVersion != baseVersion)
            {
                const int x = cell.x * side / block, y = cell.y * side / block,
                          w = std::min(side / block, overviewW - x),
                          h = std::min(side / block, overviewH - y);
                std::vector<RenderColor> pixels(std::size_t(w) * h);
                for (int yy = 0; yy < h; ++yy)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        pixels[std::size_t(yy) * w + xx] =
                            overviewPixel(x + xx, y + yy);
                    }
                }
                renderer
                    .updateTextureRegion(*overviewTexture_, x, y, w, h, pixels);
                chunk.baseVersion = baseVersion;
            }
            if (sprites && policy && tp >= 4 && detail < 1 &&
                (chunk.version != version ||
                 chunk.navigation != map.objectState().navigationVersion()))
            {
                if (map.naturalFeatures().countIn(
                        {{cell.x * side - 2, cell.y * side - 2},
                         side + 4,
                         side + 4}
                    ) == 0)
                {
                    textures -= bool(chunk.texture);
                    chunk.texture.reset();
                    chunk.empty = true;
                    chunk.version = version;
                    chunk.navigation = map.objectState().navigationVersion();
                    continue;
                }
                // Cache the same silhouettes and placement as the close view.
                // Only animation is omitted; no replacement circles or fade-in.
                textures -= bool(chunk.texture);
                chunk.texture.reset();
                SceneDrawQueue snapshot;
                submitDetailed(
                    renderer,
                    map,
                    {(cell.x + .5) * side,
                     (cell.y + .5) * side,
                     double(density),
                     textureSide,
                     textureSide},
                    snapshot,
                    *sprites,
                    *policy
                );
                auto ordered = snapshot.items();
                std::stable_sort(
                    ordered.begin(),
                    ordered.end(),
                    SceneDrawQueue::before
                );
                chunk.commands.clear();
                for (const auto& item : ordered)
                {
                    const auto& d = item.bounds;
                    const float l = std::max(0.f, d.x),
                                top = std::max(0.f, d.y);
                    const float r = std::min(float(textureSide), d.x + d.width),
                                bottom = std::min(
                                    float(textureSide),
                                    d.y + d.height
                                );
                    if (r <= l || bottom <= top)
                    {
                        continue;
                    }
                    auto f = item.atlasFrame;
                    if (item.texture)
                    {
                        f.x += (l - d.x) * f.width / d.width;
                        f.y += (top - d.y) * f.height / d.height;
                        f.width *= (r - l) / d.width;
                        f.height *= (bottom - top) / d.height;
                    }
                    chunk.commands.push_back(
                        {item.texture,
                         f,
                         {l, top, r - l, bottom - top},
                         item.color,
                         item.opacity}
                    );
                }
                std::unique_ptr<Texture> image;
                if (textures < 256 && built < 2 && SDL_GetTicksNS() < deadline)
                {
                    image = renderer.createTextureFromDrawItems(
                        textureSide,
                        textureSide,
                        chunk.commands
                    );
                }
                chunk.empty = false;
                chunk.version = version;
                chunk.navigation = map.objectState().navigationVersion();
                if (image)
                {
                    if (!chunk.texture)
                    {
                        chunk.readyAt = SDL_GetTicksNS() / 1e9;
                    }
                    textures += !chunk.texture;
                    chunk.texture = std::move(image);
                    chunk.empty = false;
                    chunk.version = version;
                    chunk.navigation = map.objectState().navigationVersion();
                }
                ++built;
            }
            if (chunk.empty && chunk.version == version &&
                chunk.navigation == map.objectState().navigationVersion())
            {
                continue;
            }
            if (!chunk.texture && !chunk.commands.empty() && textures < 256 && built < 2 &&
                SDL_GetTicksNS() < deadline)
            {
                chunk.texture = renderer.createTextureFromDrawItems(
                    textureSide,
                    textureSide,
                    chunk.commands
                );
                textures += bool(chunk.texture);
                ++built;
            }
            const auto draw = [&](const Texture& texture,
                                  float sx,
                                  float sy,
                                  float sw,
                                  float sh,
                                  double alpha)
            {
                if (alpha > 0)
                {
                    renderer.drawTexture(
                        texture,
                        sx,
                        sy,
                        sw,
                        sh,
                        float(ox + cell.x * side * tp),
                        float(oy + cell.y * side * tp),
                        float(side * tp),
                        float(side * tp),
                        std::uint8_t(std::round(255 * alpha))
                    );
                }
            };
            if (detail == 0)
            {
                if (chunk.texture)
                {
                    draw(*chunk.texture, 0, 0, textureSide, textureSide, 1);
                }
                else
                {
                    for (const auto& item : chunk.commands)
                    {
                        const auto& d = item.destination;
                        const float scale = float(tp / density);
                        const float x = float(ox + cell.x * side * tp) +
                                        d.x * scale,
                                    y = float(oy + cell.y * side * tp) +
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
                                item.opacity
                            );
                        }
                        else
                        {
                            renderer.fillRectangle(
                                x,
                                y,
                                d.width * scale,
                                d.height * scale,
                                item.fill
                            );
                        }
                    }
                }
            }
        }
        if (shared && sprites && policy && detail > 0)
        {
            const auto begin = shared->size();
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
            shared->setOpacityFrom(begin, detail);
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
        const auto cellKey = [](int x, int y)
        { return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y); };
        if (clearanceInstance_ != map.instanceId() ||
            clearanceVersion_ != map.objectState().navigationVersion() ||
            clearanceArtEnabled_ != SceneSpriteLibrary::environmentArtEnabled())
        {
            clearanceArtEnabled_ = SceneSpriteLibrary::environmentArtEnabled();
            clearanceInstance_ = map.instanceId();
            clearanceVersion_ = map.objectState().navigationVersion();
            clearance_.clear();
            for (const auto& o : map.objectState().completedObjects())
            {
                const auto& style = sprites.objectStyle(o.objectTypeId);
                if (style.mode != "enclosed" && style.mode != "modules" &&
                    style.mode != "single")
                {
                    continue;
                }
                const auto& f = o.footprint;
                double left = f.topLeft.x, top = f.topLeft.y - style.height;
                double right = f.topLeft.x + f.width,
                       bottom = f.topLeft.y + f.height;
                if (const auto* roof =
                        sprites.find(o.objectTypeId + ".roof.full"))
                {
                    if (sprites.find(o.objectTypeId + ".wall.front"))
                    {
                        // Match the single fitted blueprint roof, including
                        // the overhang on both sides and its raised top edge.
                        left -= .22;
                        right += .22;
                        top = std::min(top, f.topLeft.y - roof->elevation);
                    }
                    else
                    {
                        left = std::min(
                            left,
                            f.topLeft.x - roof->width * roof->pivotX
                        );
                        top = std::min(
                            top,
                            f.topLeft.y - roof->elevation -
                                roof->height * roof->pivotY
                        );
                        right = std::max(
                            right,
                            left + roof->width + f.width - style.moduleWidth
                        );
                    }
                }
                const RenderRectangle r{
                    float(left - .08),
                    float(top - .08),
                    float(right - left + .16),
                    float(bottom - top + .16)
                };
                for (int cy = int(std::floor((top - 3) / 32));
                     cy <= int(std::floor((bottom + 3) / 32));
                     ++cy)
                {
                    for (int cx = int(std::floor((left - 3) / 32));
                         cx <= int(std::floor((right + 3) / 32));
                         ++cx)
                    {
                        clearance_[cellKey(cx, cy)].push_back(r);
                    }
                }
            }
        }
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
        constexpr int shadowWidth = 32, shadowHeight = 16;
        const auto& shadowColor = policy.shadowColor;
        if (policy.shadowsVisible &&
            (sprites.find("tree") || sprites.find("rock")) &&
            (!contactShadow_ || contactShadowColor_.red != shadowColor.red ||
             contactShadowColor_.green != shadowColor.green ||
             contactShadowColor_.blue != shadowColor.blue ||
             contactShadowColor_.alpha != shadowColor.alpha))
        {
            // A shared lighting mask, separate from authored sprite colors.
            // Four alpha steps soften the ellipse while retaining pixel edges.
            std::array<RenderColor, shadowWidth * shadowHeight> pixels{};
            for (int y = 0; y < shadowHeight; ++y)
            {
                for (int x = 0; x < shadowWidth; ++x)
                {
                    const double nx =
                        (x + .5 - shadowWidth * .5) / (shadowWidth * .5);
                    const double ny =
                        (y + .5 - shadowHeight * .5) / (shadowHeight * .5);
                    const double coverage =
                        std::round(
                            std::clamp(
                                (1 - std::sqrt(nx * nx + ny * ny)) / .45,
                                0.0,
                                1.0
                            ) *
                            4
                        ) /
                        4;
                    auto color = shadowColor;
                    color.alpha =
                        std::uint8_t(std::round(shadowColor.alpha * coverage));
                    pixels[y * shadowWidth + x] = color;
                }
            }
            contactShadow_ = renderer.createTextureFromPixels(
                shadowWidth,
                shadowHeight,
                pixels
            );
            contactShadowColor_ = shadowColor;
        }
        constexpr int side = SettlementNaturalFeatures::ChunkSide;
        const int columns = (map.grid().width() + side - 1) / side;
        const int rows = (map.grid().height() + side - 1) / side;
        const int stride =
            std::max(1, int(std::ceil(4.0 / projection.tilePixels)));
        // Include offscreen bases whose authored silhouettes enter the view.
        // Real catalog bounds avoid scanning a 256-tile halo around ordinary
        // two-tile trees. Coarse clusters scale their overhang by the stride.
        double pad = 2;
        for (const char* name : {"tree", "rock"})
        {
            for (int variant = 0; variant <= 4; ++variant)
            {
                const auto* art = sprites.find(
                    variant ? std::string(name) + "." + std::to_string(variant)
                            : name
                );
                if (!art)
                {
                    continue;
                }
                const double left = -art->width * art->pivotX;
                const double right = left + art->width;
                const double top = -art->elevation - art->height * art->pivotY;
                const double bottom = top + art->height;
                pad = std::max(
                    pad,
                    (std::max(
                         {std::abs(left),
                          std::abs(right),
                          std::abs(top),
                          std::abs(bottom)}
                     ) +
                     .5) *
                        stride
                );
            }
        }
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
                auto& chunk = featureChunks_[std::size_t(cy) * columns + cx];
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
                    const auto* occupant =
                        map.objectState().completedObjectAt(f.tile);
                    if (occupant && occupant->objectTypeId ==
                                        SettlementObjectTypes::LoggingGrounds)
                    {
                        continue;
                    }
                    if (stride > 1 &&
                        ((f.tile.x % stride) != 0 || (f.tile.y % stride) != 0))
                    {
                        continue;
                    }
                    double x = f.tile.x + .5, y = f.tile.y + .5;
                    const auto id =
                        ((std::uint64_t(f.tile.y) * map.grid().width() +
                          f.tile.x)
                         << 3) |
                        4;
                    double treeScale = double(stride);
                    const double rockScale =
                        .68 +
                        (GenerationNoise::mix(id ^ map.generationSeed()) % 5) *
                            .12;
                    double crownScale = 1;
                    if (f.tree && sprites.find("tree.trunk.1"))
                    {
                        // Test the whole wind envelope, not just the trunk's
                        // tile. Scale only crowded trees, with no frame jitter.
                        if (const auto it = clearance_.find(
                                cellKey(f.tile.x / 32, f.tile.y / 32)
                            );
                            it != clearance_.end())
                        {
                            // Use the free side of the same occupied tile;
                            // never move the resource to another map cell.
                            double shiftX = 0, shiftY = 0;
                            for (const auto& r : it->second)
                            {
                                if (y >= r.y - 1 && y <= r.y + r.height + 2)
                                {
                                    if (x < r.x && r.x - x < 1)
                                    {
                                        shiftX = -.28;
                                    }
                                    if (x > r.x + r.width &&
                                        x - r.x - r.width < 1)
                                    {
                                        shiftX = .28;
                                    }
                                }
                                if (x >= r.x - 1 && x <= r.x + r.width + 1 &&
                                    y > r.y + r.height &&
                                    y - r.y - r.height < 2)
                                {
                                    shiftY = .28;
                                }
                            }
                            x += shiftX;
                            y += shiftY;
                            const auto overlaps =
                                [&](double scale, double crown)
                            {
                                const double half = .82 * scale * crown;
                                const double top =
                                    y - (.48 + 1.25 * crown) * scale;
                                for (const auto& r : it->second)
                                {
                                    if (x + half > r.x &&
                                        x - half < r.x + r.width && y > r.y &&
                                        top < r.y + r.height)
                                    {
                                        return true;
                                    }
                                }
                                return false;
                            };
                            while (crownScale > .41 &&
                                   overlaps(treeScale, crownScale))
                            {
                                crownScale -= .15;
                            }
                            while (treeScale > .25 &&
                                   overlaps(treeScale, crownScale))
                            {
                                treeScale = std::max(.25, treeScale * .8);
                            }
                            if (overlaps(treeScale, crownScale))
                            {
                                continue;
                            }
                        }
                    }
                    const bool custom =
                        (f.tree &&
                         sprites.submitTree(
                             queue,
                             projection,
                             x,
                             y,
                             GenerationNoise::mix(id ^ map.generationSeed()),
                             treeScale,
                             crownScale
                         )) ||
                        sprites.submit(
                            queue,
                            projection,
                            f.tree ? "tree" : "rock",
                            x,
                            y,
                            id,
                            double(stride) * (f.tree ? 1.0 : rockScale)
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
                    if (policy.shadowsVisible && custom && contactShadow_)
                    {
                        const auto shadow = projection.bounds(
                            {x + .12 * stride,
                             y + .08 * stride,
                             0,
                             (f.tree ? .95 * treeScale * crownScale
                                     : .78 * stride * rockScale),
                             (f.tree ? .28 * treeScale : .28 * stride),
                             .5,
                             .5}
                        );
                        if (projection.visible(shadow))
                        {
                            queue.submit(
                                {shadow,
                                 {},
                                 y,
                                 id,
                                 -1,
                                 0,
                                 contactShadow_.get(),
                                 {0, 0, shadowWidth, shadowHeight}}
                            );
                        }
                    }
                    if (!projection.visible(b))
                    {
                        continue;
                    }
                    if (policy.shadowsVisible && !custom)
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
