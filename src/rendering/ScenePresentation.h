#pragma once
#include "rendering/Renderer.h"
#include <algorithm>
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
    };
    // One visible-item queue; no terrain-wide sorting, per-entity textures,
    // or dependency from simulation state to SDL/resources.
    class SceneDrawQueue
    {
    public:
        void clear()
        {
            items_.clear();
        }
        void submit(const SceneDrawItem& item)
        {
            items_.push_back(item);
        }
        static bool before(const SceneDrawItem& a, const SceneDrawItem& b)
        {
            return std::tie(a.layer, a.groundDepth, a.stableId, a.part) <
                   std::tie(b.layer, b.groundDepth, b.stableId, b.part);
        }
        void render(Renderer& renderer)
        {
            std::sort(items_.begin(), items_.end(), before);
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
                const auto& b = item.bounds;
                if (item.texture)
                {
                    flush();
                    const auto& f = item.atlasFrame;
                    renderer.drawTexture(
                        *item.texture,
                        f.x,
                        f.y,
                        f.width,
                        f.height,
                        b.x,
                        b.y,
                        b.width,
                        b.height
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
        std::vector<SceneDrawItem> items_;
        std::vector<RenderRectangle> batch_;
    };
} // namespace Paladin
