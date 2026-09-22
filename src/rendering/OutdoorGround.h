#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    // Reveal the actual terrain through a broken, binary-alpha material edge.
    // Source texels keep their world phase; nothing paints flat green over grass.
    inline void outdoorGround(SceneDrawQueue& queue, const SceneProjection& view,
        const SceneSpriteLibrary& sprites, const std::string& material,
        const SceneVisual& area, RenderColor fallback, double depth,
        std::uint64_t id, int part = 0, bool placeholder = true)
    {
        const auto bounds = view.bounds(area);
        if (!view.visible(bounds) || area.width <= 0 || area.height <= 0) { return; }
        const auto* sprite = sprites.find(material);
        if (!sprite && !placeholder) { return; }
        const auto frame = sprite ? sprites.frame(*sprite, false) : RenderRectangle{};
        const double coarsePeriod = view.tilePixels < 8 ? 8 / view.tilePixels : 0;
        const double periodX = std::max(sprite ? sprite->width : 1, coarsePeriod);
        const double periodY = std::max(sprite ? sprite->height : 1, coarsePeriod);
        const double cell = view.tilePixels >= 16 ? .125 : std::max(.25, 1 / view.tilePixels);
        const double halfW = view.screenWidth * .5 / view.tilePixels;
        const double halfH = view.screenHeight * .5 / view.tilePixels;
        const double ax = area.groundX - area.width * area.pivotX;
        const double ay = area.groundY - area.height * area.pivotY;
        const double left = std::max(ax, std::floor((view.cameraX - halfW - 1) / cell) * cell);
        const double right = std::min(ax + area.width, std::ceil((view.cameraX + halfW + 1) / cell) * cell);
        const double top = std::max(ay, std::floor((view.cameraY - halfH - 1) / cell) * cell);
        const double bottom = std::min(ay + area.height, std::ceil((view.cameraY + halfH + 1) / cell) * cell);
        const auto emit = [&](double x, double y, double w, double h)
        {
            if (w <= 0 || h <= 0) { return; }
            const auto b = view.bounds({x, y, 0, w, h, 0, 0});
            if (!view.visible(b)) { return; }
            if (!sprite)
            { queue.submit({b, fallback, depth, id, 0, part}); return; }
            // Callers only hand us rectangles inside one material repeat.
            const double u = x - std::floor(x / periodX) * periodX;
            const double v = y - std::floor(y / periodY) * periodY;
            queue.submit({b, {}, depth, id, 0, part, sprite->texture.get(),
                {frame.x + float(u / periodX) * frame.width,
                 frame.y + float(v / periodY) * frame.height,
                 float(w / periodX) * frame.width, float(h / periodY) * frame.height}});
        };
        constexpr double fringe = .375;
        // Fine edge cells only where visible. Coarse views omit small edge
        // speckles instead of allocating a different texture for each zoom.
        for (double my = std::floor(top / periodY) * periodY; my < bottom; my += periodY)
        for (double mx = std::floor(left / periodX) * periodX; mx < right; mx += periodX)
        {
            const double x0 = std::max(left, mx), x1 = std::min(right, mx + periodX);
            const double y0 = std::max(top, my), y1 = std::min(bottom, my + periodY);
            if (x0 >= ax + fringe && x1 <= ax + area.width - fringe &&
                y0 >= ay + fringe && y1 <= ay + area.height - fringe)
            { emit(x0, y0, x1 - x0, y1 - y0); continue; }
            // Tile the central part in row runs; only the fringe needs small
            // cells. This stays proportional to visible ground, not site area.
            for (double yy = y0; yy < y1 - 1e-8;)
            {
                const double hh = std::min(cell, y1 - yy);
                double run = -1;
                for (double xx = x0; xx < x1 - 1e-8;)
                {
                    const double ww = std::min(cell, x1 - xx);
                    const double distance = std::min({xx + ww * .5 - ax,
                        ax + area.width - xx - ww * .5,
                        yy + hh * .5 - ay, ay + area.height - yy - hh * .5});
                    const auto hash = GenerationNoise::mix(id ^
                        std::uint64_t(std::int64_t(std::floor(xx * 16))) * 73856093ULL ^
                        std::uint64_t(std::int64_t(std::floor(yy * 16))) * 19349663ULL);
                    const bool paint = distance >= fringe ||
                        double(hash % 256) / 256.0 < std::clamp(distance / fringe, 0.0, 1.0);
                    if (paint && run < 0) { run = xx; }
                    if (!paint && run >= 0) { emit(run, yy, xx - run, hh); run = -1; }
                    xx += ww;
                }
                if (run >= 0) { emit(run, yy, x1 - run, hh); }
                yy += hh;
            }
        }
    }
}
