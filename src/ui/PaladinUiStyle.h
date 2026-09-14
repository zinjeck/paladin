#pragma once
#include "rendering/Renderer.h"
#include "ui/UiTypes.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    // Native-screen chrome. Authored colors belong to art-palette.hex.
    // Camera zoom never changes these strokes or a control's hit rectangle.
    inline void paladinFrame(Renderer& renderer, UiRectangle bounds,
                             RenderColor body, bool active = false,
                             bool inset = false)
    {
        bounds.x = std::round(bounds.x);
        bounds.y = std::round(bounds.y);
        bounds.width = std::round(bounds.width);
        bounds.height = std::round(bounds.height);
        if (bounds.width <= 0 || bounds.height <= 0) return;
        const float edge = bounds.height < 28 || bounds.width < 36 ? 1.F : 2.F;
        const auto fill = [&](float x, float y, float width, float height, RenderColor color)
        {
            if (width > 0 && height > 0)
                renderer.fillRectangle(x, y, width, height, color);
        };
        const float x = bounds.x, y = bounds.y, w = bounds.width, h = bounds.height;
        fill(x, y, w, h, {8,15,27,255});
        fill(x+edge, y+edge, w-2*edge, h-2*edge, {57,70,88,255});
        fill(x+2*edge, y+2*edge, w-4*edge, h-4*edge, body);
        const RenderColor light = active ? RenderColor{213,164,84,255}
                                         : RenderColor{154,167,175,255};
        const RenderColor dark{53,56,62,255};
        fill(x+3*edge, y+edge, w-6*edge, edge, inset ? dark : light);
        fill(x+edge, y+3*edge, edge, h-6*edge, inset ? dark : light);
        fill(x+2*edge, y+h-2*edge, w-4*edge, edge, inset ? light : dark);
        fill(x+w-2*edge, y+2*edge, edge, h-4*edge, inset ? light : dark);
        if (w < 64 || h < 32) return;
        // Mirrored corner fittings, never stretched with the panel interior.
        // The same geometry is retained in every interaction state.
        for (int corner = 0; corner < 4; ++corner)
        {
            const bool right = (corner & 1) != 0, bottom = (corner & 2) != 0;
            const float cx = right ? x+w-3*edge : x+2*edge;
            const float cy = bottom ? y+h-3*edge : y+2*edge;
            fill(cx-(right ? edge : 0), cy, 2*edge, edge, {179,122,62,255});
            fill(cx, cy-(bottom ? edge : 0), edge, 2*edge, {179,122,62,255});
            fill(cx, cy, edge, edge, {235,196,107,255});
        }
    }
}
