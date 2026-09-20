#pragma once
#include "world/SettlementTilePosition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <algorithm>
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        MarketWorkplace{SettlementObjectTypes::Market, 1, 1, 12, 16};
    struct MarketStallLayout
    {
        int columns = 1;
        int rows = 1;
        int moduleWidth = 3;
        int moduleHeight = 3;
        int offsetX = 0;
        int offsetY = 0;
    };
    inline MarketStallLayout marketStallLayout(int width, int height)
    {
        // Each cell includes a compact counter, a clear merchant tile behind
        // it, and circulation space. Rendering, preview and work share it.
        const bool rotated =
            (width / 4) * (height / 3) > (width / 3) * (height / 4);
        const int cellWidth = std::min(width, rotated ? 4 : 3);
        const int cellHeight = std::min(height, rotated ? 3 : 4);
        const int columns = std::max(1, width / std::max(1, cellWidth));
        const int rows = std::max(1, height / std::max(1, cellHeight));
        return {
            columns,
            rows,
            cellWidth,
            cellHeight,
            (width - columns * cellWidth) / 2,
            (height - rows * cellHeight) / 2
        };
    }
    inline int marketStallCount(int width, int height)
    {
        const auto layout = marketStallLayout(width, height);
        return layout.columns * layout.rows;
    }
    inline SettlementTilePosition marketStallTile(
        SettlementTilePosition origin,
        int width,
        int height,
        int index
    )
    {
        const auto layout = marketStallLayout(width, height);
        index = std::clamp(index, 0, layout.columns * layout.rows - 1);
        return {
            origin.x + layout.offsetX +
                (index % layout.columns) * layout.moduleWidth +
                layout.moduleWidth / 2,
            origin.y + layout.offsetY +
                (index / layout.columns) * layout.moduleHeight +
                std::max(0, layout.moduleHeight - 3)
        };
    }
} // namespace Paladin
