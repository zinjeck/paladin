#pragma once

#include "rendering/ScenePresentation.h"
#include "assets/PresentationData.h"
#include <cstdint>
#include <vector>

namespace Paladin
{
    // CPU alpha only, retained once at asset load. No textures or per-zoom
    // masks are allocated. Selection uses the exact queued sprite frame/pivot.
    struct SpriteSelectionMask
    {
        int width = 0, height = 0;
        std::vector<std::uint8_t> alpha;
        bool opaque(int x, int y) const noexcept
        {
            return x >= 0 && y >= 0 && x < width && y < height &&
                alpha[std::size_t(y) * width + x] >= 128;
        }
    };

    inline SpriteSelectionMask spriteSelectionMask(
        const SpriteAtlas& atlas, int x, int y, int width, int height)
    {
        SpriteSelectionMask result;
        if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
            x + width > atlas.width || y + height > atlas.height) return result;
        result.width = width; result.height = height;
        result.alpha.resize(std::size_t(width) * height);
        for (int row = 0; row < height; ++row)
            for (int col = 0; col < width; ++col)
                result.alpha[std::size_t(row) * width + col] =
                    atlas.pixels[std::size_t(y + row) * atlas.width + x + col].alpha;
        return result;
    }

    inline void renderSpriteSelection(Renderer& renderer, const SpriteSelectionMask& mask,
                                       const SceneDrawItem& item)
    {
        if (mask.alpha.empty() || item.bounds.width <= 0 || item.bounds.height <= 0) return;
        const double pitch = renderer.currentPixelPitch();
        const bool software = renderer.usesSoftwareRasterizer();
        const auto endpoint = [&](float value) -> double
        {
            if (renderer.pixelSceneActive()) return std::floor(double(value) / pitch + .50001);
            // SDL's software geometry path truncates destination vertices; GPU
            // geometry retains subpixel coordinates. Respect the active backend.
            return software ? std::trunc(value) : double(value);
        };
        const double x0 = endpoint(item.bounds.x), y0 = endpoint(item.bounds.y);
        const double x1 = software && !renderer.pixelSceneActive()
            ? x0 + std::trunc((item.bounds.x + item.bounds.width) - item.bounds.x)
            : endpoint(item.bounds.x + item.bounds.width);
        const double y1 = software && !renderer.pixelSceneActive()
            ? y0 + std::trunc((item.bounds.y + item.bounds.height) - item.bounds.y)
            : endpoint(item.bounds.y + item.bounds.height);
        const int left = int(std::floor(x0)), top = int(std::floor(y0));
        const int width = int(std::ceil(x1)) - left, height = int(std::ceil(y1)) - top;
        if (width <= 0 || height <= 0 || width > 512 || height > 512 || x1 <= x0 || y1 <= y0) return;
        const auto source = item.atlasFrame;
        const auto occupied = [&](int x, int y)
        {
            const double px = left + x + .5, py = top + y + .5;
            if (px < x0 || py < y0 || px >= x1 || py >= y1) return false;
            // Axis-aligned uniform quads are SDL texture copies on software.
            // Their nearest sampler uses a 16.16 half-step, not triangle UVs.
            const auto sample = [&](double p, double start, double extent, float sourceExtent)
            {
                if (software)
                {
                    const auto step = (std::uint64_t(sourceExtent) << 16) / std::uint64_t(extent);
                    return int((step * std::uint64_t(p-start-.5) + step/2) >> 16);
                }
                return std::min(int(sourceExtent)-1, int((p-start)*sourceExtent/extent));
            };
            const int sx = int(source.x) + sample(px, x0, x1-x0, source.width);
            const int sy = int(source.y) + sample(py, y0, y1-y0, source.height);
            return mask.opaque(sx, sy);
        };
        const int radius = std::max(1, int(std::ceil(2. / pitch)));
        for (int y = -radius; y < height + radius; ++y)
        {
            int run = -radius - 1;
            for (int x = -radius; x <= width + radius; ++x)
            {
                bool edge = false;
                if (x < width + radius && !occupied(x, y))
                    for (int dy = -radius; dy <= radius && !edge; ++dy)
                        for (int dx = -radius; dx <= radius; ++dx)
                            if (occupied(x + dx, y + dy)) { edge = true; break; }
                if (edge && run < -radius) run = x;
                if (!edge && run >= -radius)
                {
                    renderer.fillRectangle(float((left+run)*pitch), float((top+y)*pitch),
                                           float((x-run)*pitch), float(pitch), {235,196,107,255});
                    run = -radius - 1;
                }
            }
        }
    }
}
