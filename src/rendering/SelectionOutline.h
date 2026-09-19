#pragma once

#include "rendering/Renderer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    // Snap shared edges, not rectangle sizes. Textures use this same lattice
    // convention. A border must occupy whole raster cells before compositing,
    // otherwise a thin far edge disappears as the camera changes phase.
    inline std::array<RenderRectangle, 4> selectionBorder(
        RenderRectangle bounds, double pitch, float requestedPixels = 2.F)
    {
        if (!std::isfinite(pitch) || pitch < 1.) pitch = 1.;
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
            !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
            bounds.width <= 0 || bounds.height <= 0) return {};
        const auto snap = [pitch](double value)
        { return float(std::floor(value / pitch + .50001) * pitch); };
        const float left = snap(bounds.x), top = snap(bounds.y);
        const float right = std::max(left + float(pitch), snap(double(bounds.x) + bounds.width));
        const float bottom = std::max(top + float(pitch), snap(double(bounds.y) + bounds.height));
        const float width = right - left, height = bottom - top;
        const double requested = std::isfinite(requestedPixels) ? std::max(2.F, requestedPixels) : 2.;
        const float stroke = std::min({width, height, float(std::ceil(requested / pitch) * pitch)});
        return {{{left, top, width, stroke}, {left, bottom - stroke, width, stroke},
                 {left, top, stroke, height}, {right - stroke, top, stroke, height}}};
    }

    inline void drawSelectionBorder(Renderer& renderer, RenderRectangle bounds,
                                    RenderColor color, float requestedPixels = 2.F)
    {
        const auto edges = selectionBorder(bounds, renderer.currentPixelPitch(), requestedPixels);
        renderer.fillRectangles(edges, color);
    }
}
