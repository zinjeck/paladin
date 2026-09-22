#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include <cmath>

namespace Paladin
{
    inline void mineralOutcrops(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SettlementMap& map
    )
    {
        if (view.tilePixels < 8)
        {
            return;
        }
        const double halfW = view.screenWidth * .5 / view.tilePixels;
        const double halfH = view.screenHeight * .5 / view.tilePixels;
        const int left = std::max(0, int(std::floor(view.cameraX - halfW)));
        const int right =
            std::min(map.grid().width(), int(std::ceil(view.cameraX + halfW)));
        const int top = std::max(0, int(std::floor(view.cameraY - halfH)));
        const int bottom =
            std::min(map.grid().height(), int(std::ceil(view.cameraY + halfH)));
        for (int y = top; y < bottom; ++y)
        {
            for (int x = left; x < right; ++x)
            {
                const auto& tile = *map.grid().tile({x, y});
                if (tile.mineral == MineralDeposit::None ||
                    tile.terrain != TerrainType::Land ||
                    map.objectState().completedObjectAt({x, y}))
                {
                    continue;
                }
                const auto seed =
                    std::uint64_t(x) * 73856093 ^ std::uint64_t(y) * 19349663;
                const RenderColor ore = tile.mineral == MineralDeposit::Gold
                                            ? RenderColor{235, 196, 107, 255}
                                        : tile.mineral == MineralDeposit::Iron
                                            ? RenderColor{154, 167, 175, 255}
                                            : RenderColor{57, 70, 88, 255};
                for (int i = 0; i < 3; ++i)
                {
                    const double px =
                        x + double((seed >> (i * 5)) % 11 + 2) / 16;
                    const double py =
                        y + double((seed >> (i * 4 + 8)) % 11 + 2) / 16;
                    queue.submit(
                        {view.bounds({px, py, 0, .1875, .125, 0, 0}),
                         {57, 43, 60, 255},
                         py,
                         seed,
                         -2}
                    );
                    queue.submit(
                        {view.bounds({px, py, 0, .125, .0625, 0, 0}),
                         ore,
                         py,
                         seed,
                         -2,
                         1}
                    );
                }
            }
        }
    }

    inline void miningPresentation(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SceneSpriteLibrary& sprites,
        const SettlementMap& map,
        const CompletedSettlementObject& object,
        std::uint64_t id
    )
    {
        const auto* job = miningJob(object.objectTypeId);
        if (!job)
        {
            return;
        }
        const auto& f = object.footprint;
        const double depth = map.mining.depth(object);
        const double x = f.topLeft.x, y = f.topLeft.y;
        const RenderColor compoundTint =
            job->deposit == MineralDeposit::Gold
                ? RenderColor{183, 131, 80, 255}
            : job->deposit == MineralDeposit::Iron
                ? RenderColor{149, 101, 95, 255}
            : job->deposit == MineralDeposit::Coal
                ? RenderColor{57, 70, 88, 255}
                : RenderColor{113, 109, 112, 255};
        constexpr double compoundDepth = 2.0;
        // Only the permanent surface compound is cleared and resource-coded.
        // The excavation below keeps the established dark rock/strata palette.
        const auto groundStart = queue.size();
        sprites.surface(
            queue,
            view,
            "market.floor",
            {x, y, 0, double(f.width), std::min(compoundDepth, double(f.height)), 0, 0},
            compoundTint,
            y,
            id,
            0
        );
        sprites.surface(
            queue,
            view,
            "road.floor",
            {x + .25, y + 1.55, 0, f.width - .5, .45, 0, 0},
            {116, 81, 63, 255},
            y,
            id,
            1
        );
        queue.setLayerFrom(groundStart, -3);

        // A permanent, orderly surface work strip stays above the expanding cut.
        const auto part = [&](double px,
                              double py,
                              double w,
                              double h,
                              RenderColor color,
                              int layer = 0)
        {
            queue.submit(
                {view.bounds({px, py, 0, w, h, 0, 0}), color, py + h, id, layer}
            );
        };

        // Miner hut: a tiny but legible home, with foundation, plaster walls,
        // timber frame, door, window and chimney under the shared thatch roof.
        const bool compactCompound = f.width < 5;
        const double hutWidth = compactCompound ? 1.35 : 1.7;
        const double hutX = x + f.width - hutWidth - .35;
        const double hutY = y + .28;
        part(
            hutX - .08,
            hutY + .08,
            hutWidth + .15,
            1.22,
            {57, 43, 60, 135},
            -1
        );
        part(hutX, hutY + .12, hutWidth, 1.05, {215, 200, 162, 255});
        part(hutX, hutY + 1.02, hutWidth, .15, {136, 96, 68, 255}, 1);
        part(hutX + .10, hutY + .14, .10, .93, {116, 81, 63, 255}, 2);
        part(
            hutX + hutWidth - .20,
            hutY + .14,
            .10,
            .93,
            {116, 81, 63, 255},
            2
        );
        part(hutX + .18, hutY + .42, .42, .65, {73, 53, 47, 255}, 3);
        part(hutX + .25, hutY + .52, .28, .55, {57, 43, 60, 255}, 4);
        const double windowX = hutX + hutWidth - .66;
        part(windowX, hutY + .48, .38, .34, {57, 43, 60, 255}, 3);
        part(windowX + .06, hutY + .54, .26, .20, {154, 167, 175, 255}, 4);
        part(
            hutX + hutWidth - .42,
            hutY - .12,
            .20,
            .48,
            {73, 53, 47, 255},
            3
        );
        sprites.placed(
            queue,
            view,
            "roof.thatch",
            hutX - .12,
            hutY - .25,
            hutY + 1.34,
            id,
            5,
            hutWidth + .25,
            1.18
        );

        // Hoist and stores form one readable work line rather than scattered props.
        const double hoistX = x + (compactCompound ? .25 : .45);
        const double hoistY = y + .48;
        sprites.submit(
            queue,
            view,
            "mine.hoist",
            hoistX,
            hoistY,
            id,
            compactCompound ? .68 : .82
        );
        const double storeX = x + (compactCompound ? 1.02 : 1.55);
        sprites.submit(
            queue,
            view,
            "stockpile.crate",
            storeX,
            y + .70,
            id,
            compactCompound ? .60 : .72
        );
        if (!compactCompound)
        {
            sprites.submit(
                queue,
                view,
                "stockpile.crate",
                storeX + .52,
                y + .70,
                id,
                .72
            );
            part(
                storeX - .08,
                y + 1.36,
                1.18,
                .10,
                {116, 81, 63, 255},
                1
            );
        }

        // Small wheeled ore tub and its handles, authored on the native grid.
        const double cartX = compactCompound ? x + .78 : x + 2.85;
        const double cartY = compactCompound ? y + 1.22 : y + .92;
        part(cartX, cartY, .625, .5, {57, 43, 60, 255});
        part(cartX + .0625, cartY + .0625, .5, .3125, {136, 96, 68, 255}, 1);
        part(cartX - .0625, cartY + .3125, .125, .25, {8, 15, 27, 255}, 2);
        part(cartX + .5625, cartY + .3125, .125, .25, {8, 15, 27, 255}, 2);
        part(cartX + .125, cartY + .5, .0625, .375, {136, 96, 68, 255}, 2);
        part(cartX + .4375, cartY + .5, .0625, .375, {136, 96, 68, 255}, 2);
        if (depth <= 0)
        {
            return;
        }
        const double coverage = std::sqrt(depth / job->maximumDepth);
        const double rx = std::max(.3, (f.width * .5 - .2) * coverage);
        const double ry = std::max(.3, ((f.height - 2) * .5 - .2) * coverage);
        const double cx = x + f.width * .5, cy = y + 2 + (f.height - 2) * .5;
        const double wall = depth * .85;
        // A worked cut follows the rectangular site, with stable native-pixel
        // chips along all four faces. Labor expands it without changing the
        // roughness pattern or turning the excavation into a circular crater.
        constexpr double edgeBand = .375;
        constexpr double maximumChip = .1875;
        const auto edgeInset = [&](double along, std::uint64_t edge)
        {
            const auto segment =
                std::uint64_t(std::int64_t(std::floor(along / edgeBand)));
            return double(
                       GenerationNoise::mix(
                           id ^ (segment * 73856093ULL) ^ (edge * 19349663ULL)
                       ) %
                       4
                   ) /
                   16;
        };
        const double halfW = view.screenWidth * .5 / view.tilePixels;
        const double halfH = view.screenHeight * .5 / view.tilePixels;
        // Native-art columns give the cut an irregular outline without a new
        // texture every excavation tick. Work remains proportional to viewport.
        const double stride = view.tilePixels < 8 ? .5 : .125;
        const double first = std::max(
            std::floor((cx - rx) / stride) * stride,
            std::floor((view.cameraX - halfW - 1) / stride) * stride
        );
        const double last = std::min(cx + rx, view.cameraX + halfW + 1);
        const auto* strata = sprites.find("mine.strata");
        const RenderColor vein = job->deposit == MineralDeposit::Gold
                                     ? RenderColor{235, 196, 107, 255}
                                 : job->deposit == MineralDeposit::Iron
                                     ? RenderColor{154, 167, 175, 255}
                                     : RenderColor{8, 15, 27, 255};
        for (double px = first; px < last; px += stride)
        {
            const double north = cy - ry + edgeInset(px, 0);
            const double south = cy + ry - edgeInset(px, 1);
            if (south <= north || south < view.cameraY - halfH ||
                north > view.cameraY + halfH)
            {
                continue;
            }
            const auto add = [&](double top,
                                 double height,
                                 RenderColor color,
                                 int part,
                                 const Texture* texture = nullptr,
                                 RenderRectangle frame = {},
                                 std::uint8_t opacity = 255)
            {
                if (height <= 0)
                {
                    return;
                }
                // Only the two narrow side bands need row clipping. Interior
                // columns remain single draws; even enormous sites stay
                // bounded by the viewport rather than the blueprint's area.
                const bool side = px < cx - rx + maximumChip ||
                                  px + stride > cx + rx - maximumChip;
                const double bottom =
                    std::min(top + height, view.cameraY + halfH + 1);
                for (double py = std::max(top, view.cameraY - halfH - 1);
                     py < bottom;)
                {
                    const double end =
                        side ? std::min(
                                   bottom,
                                   (std::floor(py / edgeBand) + 1) * edgeBand
                               )
                             : bottom;
                    const double left =
                        std::max(px, cx - rx + (side ? edgeInset(py, 2) : 0));
                    const double right = std::min(
                        px + stride,
                        cx + rx - (side ? edgeInset(py, 3) : 0)
                    );
                    if (right > left)
                    {
                        auto crop = frame;
                        if (texture)
                        {
                            crop.x += float((left - px) / stride) * frame.width;
                            crop.y += float((py - top) / height) * frame.height;
                            crop.width *= float((right - left) / stride);
                            crop.height *= float((end - py) / height);
                        }
                        queue.submit(
                            {view.bounds(
                                 {left, py, 0, right - left, end - py, 0, 0}
                             ),
                             color,
                             cy,
                             id,
                             -2,
                             part,
                             texture,
                             crop,
                             opacity}
                        );
                    }
                    py = end;
                }
            };
            add(north, south - north, {57, 43, 60, 255}, 0);
            const double exposed = std::min(wall, (south - north) * .6);
            if (strata)
            {
                const auto frame = sprites.frame(*strata, false);
                const float sx =
                    frame.x +
                    float((std::int64_t(std::floor(px * 16)) % 32 + 32) % 32);
                add(north,
                    exposed,
                    {},
                    1,
                    strata->texture.get(),
                    {sx,
                     frame.y,
                     float(stride * 16),
                     float(std::max(1., exposed * 16))});
            }
            else
            {
                add(north, exposed, {108, 116, 122, 255}, 1);
            }
            const double floorTop = north + exposed;
            const double floorBottom = south - .0625;
            add(floorTop, floorBottom - floorTop, {113, 109, 112, 255}, 2);
            if (strata && floorBottom > floorTop)
            {
                const auto frame = sprites.frame(*strata, false);
                const float sx =
                    float((std::int64_t(std::floor(px * 16)) % 24 + 24) % 24);
                // Preserve the source stone grain at one art pixel per pixel.
                // Cropped strips follow the excavation instead of stretching a
                // square texture over it. Work is clipped to the visible floor.
                const double top = std::max(floorTop, view.cameraY - halfH - 1);
                const double bottom =
                    std::min(floorBottom, view.cameraY + halfH + 1);
                for (double py = top; py < bottom; py += 2)
                {
                    const double height = std::min(2., bottom - py);
                    add(py,
                        height,
                        {},
                        2,
                        strata->texture.get(),
                        {frame.x + sx,
                         frame.y,
                         float(stride * 16),
                         float(height * 16)},
                        64);
                }
                // Contact shadow under the cut face gives the floor a lower
                // plane. Narrow strata ledges remain within the original cut.
                add(floorTop,
                    std::min(.125, floorBottom - floorTop),
                    {57, 43, 60, 165},
                    3);
                if (exposed > .25)
                {
                    add(north + exposed * .45, .0625, {154, 167, 175, 255}, 3);
                    add(north + exposed * .5, .0625, {57, 70, 88, 255}, 3);
                }
            }
            add(south - .0625, .0625, {154, 167, 175, 255}, 3);
            if (job->deposit != MineralDeposit::None && exposed > .12)
            {
                const double seam =
                    north + exposed * (.42 + .12 * std::sin(px * 7));
                add(seam, .0625, vein, 4);
            }
        }
        const int ladders = std::clamp(f.width / 6, 1, 3);
        for (int i = 0; i < ladders; ++i)
        {
            const double lx =
                cx - rx + .35 + i * std::max(.6, 2 * rx / (ladders + 1));
            const double ly = cy - ry;
            const double length = .5 + depth * .85;
            part(lx, ly, .0625, length, {161, 116, 72, 255}, 1);
            part(lx + .3125, ly, .0625, length, {161, 116, 72, 255}, 1);
            for (double rung = 0; rung < length; rung += .1875)
            {
                part(lx, ly + rung, .375, .0625, {203, 155, 96, 255}, 2);
            }
        }
        if (job->tunnels && depth >= .98)
        {
            for (int i = 0; i < quarryEntranceCount(f.width); ++i)
            {
                const auto entrance = quarryEntrance(f.topLeft, f.width, i);
                const double tx = entrance.x + .1875;
                const double ty = cy - ry + edgeInset(tx + .325, 0) + wall - .5;
                queue.submit(
                    {view.bounds({tx, ty, 0, .65, .55, 0, 0}),
                     {136, 96, 68, 255},
                     ty,
                     id,
                     -1,
                     i * 2}
                );
                queue.submit(
                    {view.bounds({tx + .125, ty + .0625, 0, .4, .48, 0, 0}),
                     {8, 15, 27, 255},
                     ty,
                     id,
                     -1,
                     i * 2 + 1}
                );
            }
        }
    }
} // namespace Paladin
