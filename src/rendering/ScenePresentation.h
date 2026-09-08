#pragma once
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

namespace Paladin
{
    // Simulation coordinates stay flat. Presentation alone owns sprite height,
    // ground anchor, pivot and atlas frame. Tall art can overhang its
    // footprint.
    struct SceneVisual
    {
        double groundX = 0, groundY = 0, elevation = 0;
        double width = 1, height = 1;
        double pivotX = .5, pivotY = 1;
    };
    struct SceneProjection
    {
        double cameraX = 0, cameraY = 0, tilePixels = 1;
        int screenWidth = 0, screenHeight = 0;
        RenderRectangle bounds(const SceneVisual& visual) const
        {
            return {
                float(
                    screenWidth * .5 +
                    (visual.groundX - cameraX - visual.width * visual.pivotX) *
                        tilePixels
                ),
                float(
                    screenHeight * .5 +
                    (visual.groundY - cameraY - visual.elevation -
                     visual.height * visual.pivotY) *
                        tilePixels
                ),
                float(visual.width * tilePixels),
                float(visual.height * tilePixels)
            };
        }
        bool visible(const RenderRectangle& r) const
        {
            return r.x + r.width >= 0 && r.y + r.height >= 0 &&
                   r.x < screenWidth && r.y < screenHeight;
        }
    };
    struct SceneDrawItem
    {
        RenderRectangle bounds;
        RenderColor color;
        double groundDepth = 0;
        std::uint64_t stableId = 0;
        int layer = 0;
        int part = 0;
        const Texture* texture = nullptr;
        RenderRectangle atlasFrame;
        std::uint8_t opacity = 255;
        float thatchPixelPitch = 0;
        bool ridgeAlongDepth = false;
        double windSeconds = 0;
    };
    // One visible-item queue; no terrain-wide sorting, per-entity textures,
    // or dependency from simulation state to SDL/resources.
    class SceneDrawQueue
    {
    public:
        std::size_t size() const
        {
            return items_.size();
        }
        void bendFrom(
            std::size_t first,
            double dx,
            double scaleY,
            double groundY
        )
        {
            for (auto i = first; i < items_.size(); ++i)
            {
                auto& b = items_[i].bounds;
                b.x += float(dx);
                b.y = float(groundY + (b.y - groundY) * scaleY);
                b.height *= float(scaleY);
            }
        }
        const std::vector<SceneDrawItem>& items() const
        {
            return items_;
        }
        void setLayerFrom(std::size_t first, int layer)
        {
            sorted_ = false;
            for (; first < items_.size(); ++first)
            {
                items_[first].layer = layer;
            }
        }
        void setOpacityFrom(std::size_t first, double opacity)
        {
            for (; first < items_.size(); ++first)
            {
                if (items_[first].texture)
                {
                    items_[first].opacity =
                        std::uint8_t(items_[first].opacity * opacity);
                }
                else
                {
                    items_[first].color.alpha =
                        std::uint8_t(items_[first].color.alpha * opacity);
                }
            }
        }
        void clear()
        {
            items_.clear();
            sorted_ = false;
        }
        void submit(const SceneDrawItem& item)
        {
            items_.push_back(item);
            sorted_ = false;
        }
        static bool before(const SceneDrawItem& a, const SceneDrawItem& b)
        {
            return std::tie(a.layer, a.groundDepth, a.stableId, a.part) <
                   std::tie(b.layer, b.groundDepth, b.stableId, b.part);
        }
        void render(
            Renderer& renderer,
            int minimumLayer = std::numeric_limits<int>::min(),
            int maximumLayer = std::numeric_limits<int>::max()
        )
        {
            if (!sorted_)
            {
                std::stable_sort(items_.begin(), items_.end(), before);
                sorted_ = true;
            }
            batch_.clear();
            RenderColor batchColor;
            const auto flush = [&]()
            {
                if (!batch_.empty())
                {
                    renderer.fillRectangles(batch_, batchColor);
                    batch_.clear();
                }
            };
            for (const auto& item : items_)
            {
                if (item.layer < minimumLayer || item.layer > maximumLayer)
                {
                    continue;
                }
                const auto& b = item.bounds;
                if (item.texture)
                {
                    flush();
                    const auto& f = item.atlasFrame;
                    if (item.thatchPixelPitch > 0)
                    {
                        drawThatch(renderer, item);
                        continue;
                    }
                    renderer.drawTexture(
                        *item.texture,
                        f.x,
                        f.y,
                        f.width,
                        f.height,
                        b.x,
                        b.y,
                        b.width,
                        b.height,
                        item.opacity
                    );
                }
                else
                {
                    const auto& c = item.color;
                    if (std::tie(c.red, c.green, c.blue, c.alpha) !=
                        std::tie(
                            batchColor.red,
                            batchColor.green,
                            batchColor.blue,
                            batchColor.alpha
                        ))
                    {
                        flush();
                    }
                    batchColor = c;
                    batch_.push_back(b);
                }
            }
            flush();
        }

    private:
        // Roofs share a presentation mesh; simulation bounds and source assets
        // stay unchanged. Reuse scratch storage instead of allocating textures
        // or submitting a draw call for every straw strip.
        void drawThatch(Renderer& renderer, const SceneDrawItem& item)
        {
            roofVertices_.clear();
            roofIndices_.clear();
            const auto& b = item.bounds;
            const auto& f = item.atlasFrame;
            const bool side = item.ridgeAlongDepth;
            const float length = side ? b.height : b.width;
            const float breadth = side ? b.width : b.height;
            const int columns = std::clamp(
                int(std::round(length / item.thatchPixelPitch)),
                8,
                512
            );
            const float pixel = item.thatchPixelPitch / breadth;
            const auto vertex =
                [&](float u, float v, float tu, float tv, int shade)
            {
                if (side)
                {
                    std::swap(u, v);
                    std::swap(tu, tv);
                }
                return MeshVertex{
                    b.x + b.width * u,
                    b.y + b.height * v,
                    (f.x + f.width * tu) / item.texture->width(),
                    (f.y + f.height * tv) / item.texture->height(),
                    {std::uint8_t(shade),
                     std::uint8_t(shade),
                     std::uint8_t(shade),
                     item.opacity}
                };
            };
            const auto quad = [&](float l,
                                  float r,
                                  float top,
                                  float bottom,
                                  float st,
                                  float sb,
                                  int shade)
            {
                const int base = int(roofVertices_.size());
                roofVertices_.push_back(vertex(l, top, l, st, shade));
                roofVertices_.push_back(vertex(r, top, r, st, shade));
                roofVertices_.push_back(vertex(r, bottom, r, sb, shade));
                roofVertices_.push_back(vertex(l, bottom, l, sb, shade));
                for (int i : {0, 1, 2, 0, 2, 3})
                {
                    roofIndices_.push_back(base + i);
                }
            };
            for (int col = 0; col < columns; ++col)
            {
                const float l = float(col) / columns,
                            r = float(col + 1) / columns;
                const float u = (l + r) * .5F;
                const float hip =
                    std::max(0.F, 1.F - std::min(u, 1 - u) / .24F);
                const auto hash = std::uint32_t(
                    col * 2654435761u + item.stableId * 2246822519u
                );
                // Ragged ends are small, stable whole art pixels, never
                // random frame-to-frame changes to the structural roof.
                // The front elevation has a foreshortened rear slope. A
                // depth-facing ridge instead needs two balanced roof planes;
                // transposing that .29 ridge made a lopsided slab in side view.
                const float ridge = side ? .5F : .29F;
                // In side view the near eave still covers the full front
                // wall. Only the far end foreshortens; clipping both ends
                // exposes the front wall corners beneath the roof.
                const float silhouetteHip =
                    side ? std::max(0.F, 1.F - u / .24F) : hip;
                const float inset =
                    std::round((.21F * silhouetteHip) / pixel) * pixel;
                const float fringe = (hash >> 27) % 3 * pixel;
                const float wind =
                    item.windSeconds > 0 &&
                            std::sin(
                                item.windSeconds * 2.1 + (hash % 97) * .31
                            ) > .85
                        ? pixel
                        : 0;
                const float top = inset + (side ? fringe + wind : 0);
                const float bottom = 1 - fringe - wind - (side ? inset : 0);
                quad(
                    l,
                    r,
                    top,
                    ridge,
                    0,
                    .5F,
                    side ? (hip > .05F ? 218 : 250) : (hip > .05F ? 190 : 213)
                );
                const float seam = ridge + (bottom - ridge) * hip;
                if (hip > 0)
                {
                    quad(l, r, ridge, seam, .5F, .5F + .5F * hip, 205);
                }
                if (hip < 1)
                {
                    quad(
                        l,
                        r,
                        seam,
                        bottom,
                        .5F + .5F * hip,
                        1,
                        side ? 215 : 255
                    );
                }
                // A narrow binding catches the light at the pitched ridge.
                if (!side || hip < .05F)
                {
                    quad(l, r, ridge - pixel, ridge, .46F, .49F, 255);
                }
            }
            renderer.drawMesh(*item.texture, roofVertices_, roofIndices_);
        }
        std::vector<MeshVertex> roofVertices_;
        std::vector<int> roofIndices_;
        bool sorted_ = false;
        std::vector<SceneDrawItem> items_;
        std::vector<RenderRectangle> batch_;
    };
} // namespace Paladin
