#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"

namespace Paladin
{
    // Flush foundations occupy only the boundary of the existing footprint.
    // Visible edge segments, not the whole yard area, bound rendering work.
    inline void workYardFoundation(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SettlementMap& map,
        const CompletedSettlementObject& object,
        std::uint64_t id,
        bool timber
    )
    {
        if (view.tilePixels < 8)
        {
            return;
        }
        const auto& f = object.footprint;
        const double x = f.topLeft.x, y = f.topLeft.y;
        const double halfW = view.screenWidth * .5 / view.tilePixels + 1;
        const double halfH = view.screenHeight * .5 / view.tilePixels + 1;
        const auto block = [&](double px,
                               double py,
                               double w,
                               double h,
                               RenderColor color,
                               int part)
        {
            const auto bounds = view.bounds({px, py, 0, w, h, 0, 0});
            if (view.visible(bounds))
            {
                queue.submit({bounds, color, y + f.height, id, -2, part});
            }
        };
        for (int edge = 0; edge < 4; ++edge)
        {
            const bool vertical = edge % 2;
            const double length = vertical ? f.height : f.width;
            const double origin = vertical ? y : x;
            const double low =
                (vertical ? view.cameraY - halfH : view.cameraX - halfW) -
                origin;
            const double high =
                (vertical ? view.cameraY + halfH : view.cameraX + halfW) -
                origin;
            const int first = std::max(0, int(std::floor(low * 2)));
            const int last =
                std::min(int(length * 2), int(std::ceil(high * 2)));
            for (int segment = first; segment < last; ++segment)
            {
                const double along = segment * .5;
                const double px = edge == 1   ? x + f.width - .125
                                  : edge == 3 ? x
                                              : x + along;
                const double py = edge == 2   ? y + f.height - .125
                                  : edge == 0 ? y
                                              : y + along;
                const double w = vertical ? .125 : .5, h = vertical ? .5 : .125;
                const auto seed = (id * 73856093ULL) ^
                                  std::uint64_t(segment * 31 + edge * 137);
                // Narrow exposed sill with a lit cap and dark end grain.
                block(
                    px,
                    py,
                    w,
                    h,
                    timber ? RenderColor{99, 62, 75, 255}
                           : RenderColor{108, 116, 122, 255},
                    30
                );
                block(
                    px,
                    py,
                    vertical ? .0625 : w,
                    vertical ? h : .0625,
                    timber ? RenderColor{167, 141, 114, 255}
                           : RenderColor{154, 167, 175, 255},
                    31
                );
                const double ox = edge == 1 ? .125 : edge == 3 ? -.125 : 0;
                const double oy = edge == 2 ? .125 : edge == 0 ? -.125 : 0;
                const double gx = px + ox, gy = py + oy;
                const auto* ground =
                    map.grid().tile({int(std::floor(gx)), int(std::floor(gy))});
                if (ground && ground->terrain == TerrainType::Land)
                {
                    const double clod = .0625 * (1 + seed % 2);
                    block(
                        gx,
                        gy,
                        vertical ? clod : .3125,
                        vertical ? .3125 : clod,
                        {116, 81, 63, 210},
                        32
                    );
                    block(
                        gx + .0625,
                        gy + .0625,
                        .125,
                        .0625,
                        {121, 181, 109, 255},
                        33
                    );
                }
                if (timber && segment % 3 == 0)
                {
                    // Driven pegs rise only four art pixels above their foot.
                    block(
                        px - .0625,
                        py - .125,
                        .1875,
                        .25,
                        {99, 62, 75, 255},
                        34
                    );
                    block(px, py - .125, .0625, .1875, {189, 134, 76, 255}, 35);
                    block(px, py - .125, .125, .0625, {213, 164, 84, 255}, 36);
                }
            }
        }
    }
} // namespace Paladin
