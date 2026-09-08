#pragma once
#include "rendering/SceneSpriteLibrary.h"

namespace Paladin
{
    // Shared adobe-and-timber construction. Dimensions are world tiles; every
    // bevel, joint and grain mark is snapped to the canonical 1/16 tile grid.
    // A wall is a raised cap plus a vertical face, not a floor-texture tile.
    inline void framedWall(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const std::string& material,
        double x,
        double y,
        double w,
        double d,
        double height,
        double depth,
        std::uint64_t id,
        int part,
        bool posts,
        bool lowCut = false
    )
    {
        constexpr double px = 1. / 16.;
        height = std::round(height * 16) / 16.;
        if (!p.visible(p.bounds(
                {x - px,
                 y - height - px,
                 0,
                 w + 2 * px,
                 d + height + 4 * px,
                 0,
                 0}
            )))
        {
            return;
        }
        const auto rect =
            [&](double xx, double yy, double ww, double hh, RenderColor color)
        {
            if (ww <= 0 || hh <= 0)
            {
                return;
            }
            const auto bounds = p.bounds({xx, yy, 0, ww, hh, 0, 0});
            if (p.visible(bounds))
            {
                queue.submit({bounds, color, depth, id, 0, part});
            }
        };
        const auto* wallMaterial = sprites.find(material);
        const RenderColor cap = wallMaterial && wallMaterial->materialBase.alpha
                                    ? wallMaterial->materialBase
                                    : RenderColor{167, 141, 114, 255};
        const RenderColor dark{78, 59, 57, 255}, wood{122, 80, 56, 255},
            woodLight{183, 131, 80, 255}, clay{167, 141, 114, 255},
            clayLight{193, 139, 90, 255}, clayShade{116, 81, 63, 255};
        // Earth footing projects beyond the plaster. Irregular compacted-clay
        // courses and small worn corners make contact with the terrain.
        rect(x, y + d - px, w, 3 * px, clayShade);
        for (int i = 0; i < int(std::ceil(w * 2)); ++i)
        {
            const double xx = x + i * .5;
            const double ww = std::min(.5, x + w - xx);
            const double wear = (i % 3 == 1 ? px : 0);
            rect(xx, y + d - px, ww - px, 2 * px, clayLight);
            rect(xx + px, y + d, ww - px, 2 * px - wear, clayShade);
            rect(xx + px, y + d, ww - 2 * px, px, clay);
        }
        // Textured vertical plaster face, above the projecting earth sill.
        if (height > px)
        {
            sprites.surface(
                queue,
                p,
                material,
                {x, y + d - px, 0, w, height - px, 0, 1},
                {},
                depth,
                id,
                part,
                false
            );
        }
        if (sprites.shadowsEnabled())
        {
            // The vertical plane has a recessed foot and a softly occluded
            // upper joint. These remain on the face, not on the flat cap.
            rect(x, y + d - 2 * px, w, px, {57, 43, 60, 58});
            if (height > .375)
            {
                rect(x, y + d - height, w, px, {57, 43, 60, 44});
            }
        }
        // Low sections receive one continuous U-shaped cap after all faces;
        // no segment end, raised post or timber divider crosses that surface.
        if (lowCut)
        {
            return;
        }
        // A recessed, broken clay core between raised timber rails. The
        // surface detail is seeded in world pixels, not repeated per tile.
        if (d > 0)
        {
            rect(x, y - height, w, d, dark);
            rect(x + px, y - height + px, w - 2 * px, d - 2 * px, clayShade);
            rect(x + 2 * px, y - height + px, w - 4 * px, d - 3 * px, cap);
            const int left = std::max(
                2,
                int((p.cameraX - p.screenWidth * .5 / p.tilePixels - x) * 16)
            );
            const int right = std::min(
                int(w * 16) - 2,
                int((p.cameraX + p.screenWidth * .5 / p.tilePixels - x) * 16) +
                    1
            );
            const int first = std::max(
                2,
                int((p.cameraY - p.screenHeight * .5 / p.tilePixels - y +
                     height) *
                    16)
            );
            const int last = std::min(
                int(d * 16) - 2,
                int((p.cameraY + p.screenHeight * .5 / p.tilePixels - y +
                     height) *
                    16) +
                    1
            );
            for (int yy = first; yy < last; ++yy)
            {
                for (int xx = left; xx < right; ++xx)
                {
                    const unsigned seed =
                        unsigned(int(x * 16) + xx) * 374761393u ^
                        unsigned(int(y * 16) + yy) * 668265263u;
                    const unsigned hash = (seed ^ (seed >> 13)) * 1274126177u;
                    if ((hash & 31) < 3)
                    {
                        rect(
                            x + xx * px,
                            y - height + yy * px,
                            px,
                            px,
                            (hash & 32) ? clayLight : clayShade
                        );
                    }
                }
            }
            // Raised inner face at the cut edge casts a narrow contact shadow.
            rect(
                x + 2 * px,
                y + d - height - 3 * px,
                w - 4 * px,
                px,
                clayLight
            );
            if (!posts)
            {
                // The sides expose the shaded vertical section along the
                // room edge, with sawn timber ends at structural bay intervals.
                rect(
                    x + w - 3 * px,
                    y - height + px,
                    2 * px,
                    d - px,
                    clayShade
                );
                for (int bay = 0; bay < int(d * 16); bay += 32)
                {
                    rect(x, y - height + bay * px, w, 2 * px, wood);
                    rect(
                        x + px,
                        y - height + bay * px,
                        w - 2 * px,
                        px,
                        woodLight
                    );
                }
            }
        }
        // The two rails and their opposite lighting establish cap thickness.
        rect(x, y - height, w, px, woodLight);
        rect(x, y + d - height - px, w, 2 * px, dark);
        rect(x + px, y + d - height - px, w - 2 * px, px, wood);
        rect(x, y - height, px, d, woodLight);
        rect(x + w - px, y - height, px, d + px, dark);
        if (d > .5)
        {
            rect(x + px, y - height + px, px, d - 2 * px, wood);
            rect(x + w - 2 * px, y - height + px, px, d - 2 * px, wood);
        }
        // Sill beam has a lit top and a darker lower plane.
        rect(x, y + d - 2 * px, w, px, wood);
        rect(x, y + d - px, w, px, dark);
        if (!posts)
        {
            return;
        }
        // Repeated structural bays, with end posts shared at corners. Raised
        // end-grain caps, lit edges and a recessed side plane give real volume.
        const int bays = std::max(1, int(std::ceil(w / 2.)));
        for (int i = 0; i <= bays; ++i)
        {
            const double xx = x + std::round((w - 4 * px) * i / bays * 16) / 16;
            const double top = y + d - height - 2 * px;
            if (sprites.shadowsEnabled())
            {
                rect(xx + 4 * px, top + px, px, height, {57, 43, 60, 62});
            }
            rect(xx, top, 4 * px, height + 2 * px, dark);
            rect(xx, top, 3 * px, height + px, wood);
            rect(xx, top + px, px, height, woodLight);
            rect(xx - px, top, 5 * px, px, woodLight);
            rect(xx, top - px, 3 * px, px, clayLight);
            for (int grain = 2; grain < int(height * 16); grain += 4)
            {
                rect(xx + px, top + grain * px, px, 2 * px, clayShade);
            }
            if (height >= .5)
            {
                // Short stepped timber knee braces, never a one-pixel line.
                for (int step = 0; step < 3; ++step)
                {
                    if (i < bays)
                    {
                        rect(
                            xx + 3 * px + step * px,
                            top + (3 - step) * px,
                            px,
                            2 * px,
                            wood
                        );
                    }
                    if (i > 0)
                    {
                        rect(
                            xx - (step + 1) * px,
                            top + (3 - step) * px,
                            px,
                            2 * px,
                            wood
                        );
                    }
                }
            }
        }
    }
} // namespace Paladin
