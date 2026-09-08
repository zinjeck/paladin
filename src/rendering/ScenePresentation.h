#pragma once
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace Paladin
{
    inline thread_local int activeSceneQuarterTurns = 0;

    inline int normalizedSceneQuarterTurns(int turns) noexcept
    {
        turns %= 4;
        return turns < 0 ? turns + 4 : turns;
    }

    class SceneQuarterTurnScope
    {
    public:
        explicit SceneQuarterTurnScope(int turns) noexcept
            : previous_(activeSceneQuarterTurns)
        {
            activeSceneQuarterTurns = normalizedSceneQuarterTurns(turns);
        }
        ~SceneQuarterTurnScope()
        {
            activeSceneQuarterTurns = previous_;
        }
        SceneQuarterTurnScope(const SceneQuarterTurnScope&) = delete;
        SceneQuarterTurnScope& operator=(const SceneQuarterTurnScope&) = delete;

    private:
        int previous_ = 0;
    };

    inline std::pair<double, double> rotateSceneOffset(
        double x,
        double y,
        int quarterTurns = activeSceneQuarterTurns
    ) noexcept
    {
        switch (normalizedSceneQuarterTurns(quarterTurns))
        {
        case 1:
            return {y, -x};
        case 2:
            return {-x, -y};
        case 3:
            return {-y, x};
        default:
            return {x, y};
        }
    }

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
        int quarterTurns = activeSceneQuarterTurns;

        [[nodiscard]]
        std::pair<double, double> viewOffset(double x, double y) const noexcept
        {
            return rotateSceneOffset(x, y, quarterTurns);
        }

        [[nodiscard]]
        RenderRectangle groundBounds(
            double x,
            double y,
            double width,
            double height
        ) const
        {
            const auto a = viewOffset(x - cameraX, y - cameraY);
            const auto b = viewOffset(x + width - cameraX, y - cameraY);
            const auto c = viewOffset(
                x + width - cameraX,
                y + height - cameraY
            );
            const auto d = viewOffset(x - cameraX, y + height - cameraY);
            const double minX = std::min({a.first, b.first, c.first, d.first});
            const double maxX = std::max({a.first, b.first, c.first, d.first});
            const double minY = std::min({a.second, b.second, c.second, d.second});
            const double maxY = std::max({a.second, b.second, c.second, d.second});
            return {
                float(screenWidth * .5 + minX * tilePixels),
                float(screenHeight * .5 + minY * tilePixels),
                float((maxX - minX) * tilePixels),
                float((maxY - minY) * tilePixels)
            };
        }

        RenderRectangle bounds(const SceneVisual& visual) const
        {
            const auto offset = viewOffset(
                visual.groundX - cameraX,
                visual.groundY - cameraY
            );
            const double anchorX = screenWidth * .5 + offset.first * tilePixels;
            const double anchorY = screenHeight * .5 + offset.second * tilePixels;
            return {
                float(anchorX - visual.width * visual.pivotX * tilePixels),
                float(
                    anchorY -
                    (visual.elevation + visual.height * visual.pivotY) *
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
                if (activeSceneQuarterTurns)
                {
                    items_[i].groundDepth = b.y + b.height;
                }
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
            auto submitted = item;
            if (activeSceneQuarterTurns)
            {
                submitted.groundDepth =
                    submitted.bounds.y + submitted.bounds.height;
            }
            items_.push_back(std::move(submitted));
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
        );

    private:
        // Roofs share a presentation mesh; simulation bounds and source assets
        // stay unchanged. Reuse scratch storage instead of allocating textures
        // or submitting a draw call for every straw strip.
        void drawThatch(Renderer& renderer, const SceneDrawItem& item);
        std::vector<MeshVertex> roofVertices_;
        std::vector<int> roofIndices_;
        bool sorted_ = false;
        std::vector<SceneDrawItem> items_;
        std::vector<RenderRectangle> batch_;
    };
} // namespace Paladin
