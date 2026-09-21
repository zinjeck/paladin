#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"

namespace Paladin
{
    inline void marketStalls(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SceneSpriteLibrary& sprites,
        const SettlementObjectFootprint& footprint,
        std::uint64_t id
    )
    {
        if (footprint.width < 3 || footprint.height < 3)
        {
            return;
        }
        const auto layout =
            marketStallLayout(footprint.width, footprint.height);
        const double halfW = view.screenWidth * .5 / view.tilePixels + 2;
        const double halfH = view.screenHeight * .5 / view.tilePixels + 2;
        const int x = footprint.topLeft.x + layout.offsetX;
        const int y = footprint.topLeft.y + layout.offsetY;
        const auto first = [](double coordinate, int step, int count)
        { return std::clamp(int(std::floor(coordinate / step)), 0, count); };
        const int firstX =
            first(view.cameraX - halfW - x, layout.moduleWidth, layout.columns);
        const int lastX = first(
                              view.cameraX + halfW - x,
                              layout.moduleWidth,
                              layout.columns - 1
                          ) +
                          1;
        const int firstY =
            first(view.cameraY - halfH - y, layout.moduleHeight, layout.rows);
        const int lastY = first(
                              view.cameraY + halfH - y,
                              layout.moduleHeight,
                              layout.rows - 1
                          ) +
                          1;
        // Distant enormous markets thin their furniture display, never the
        // real job count. Close and normal views show every visible counter.
        const int count = (lastX - firstX) * (lastY - firstY);
        const int step =
            count > 2048 ? int(std::ceil(std::sqrt(double(count) / 2048))) : 1;
        for (int row = firstY; row < lastY; row += step)
        {
            for (int column = firstX; column < lastX; column += step)
            {
                const int ordinal = row * layout.columns + column;
                const auto merchant = marketStallTile(
                    footprint.topLeft,
                    footprint.width,
                    footprint.height,
                    ordinal
                );
                // Worn lanes join adjacent counters; no enclosing metal frame.
                const double laneX = x + column * layout.moduleWidth;
                const double laneY = y + (row + 1) * layout.moduleHeight - .5;
                const auto lane = view.bounds(
                    {laneX, laneY, 0, double(layout.moduleWidth), .375, 0, 0}
                );
                queue.submit(
                    {lane, {189, 165, 137, 150}, laneY, id, -1, ordinal}
                );
                if ((ordinal % 3) == 0)
                {
                    sprites.placed(
                        queue,
                        view,
                        "stockpile.crate",
                        laneX + layout.moduleWidth - .35,
                        merchant.y + 1.5,
                        merchant.y + 1.5,
                        id,
                        ordinal * 2 + 1,
                        .42,
                        .32
                    );
                }
                else if ((ordinal % 3) == 1)
                {
                    sprites.placed(
                        queue,
                        view,
                        "home.detail.jars",
                        laneX + layout.moduleWidth - .4,
                        merchant.y + 1.5,
                        merchant.y + 1.5,
                        id,
                        ordinal * 2 + 1,
                        .4,
                        .4
                    );
                }
                sprites.placed(
                    queue,
                    view,
                    "market.stall",
                    merchant.x + .5,
                    merchant.y + 1.,
                    merchant.y + 1.,
                    id,
                    ordinal
                );
            }
        }
    }
} // namespace Paladin
