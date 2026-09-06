#pragma once
#include "world/SettlementTilePosition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <algorithm>
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        MarketWorkplace{SettlementObjectTypes::Market, 1, 1, 12, 16};
    inline int marketStallCount(int width, int height)
    {
        // A counter occupies a 3x4 module; rotate the modules for best fit.
        return std::max(
            {1, (width / 3) * (height / 4), (width / 4) * (height / 3)}
        );
    }
    inline SettlementTilePosition marketStallTile(
        SettlementTilePosition origin,
        int width,
        int height,
        int index
    )
    {
        const bool rotated =
            (width / 4) * (height / 3) >= (width / 3) * (height / 4);
        const int moduleWidth = rotated ? 4 : 3, moduleHeight = rotated ? 3 : 4;
        const int columns = std::max(1, width / moduleWidth);
        return {
            origin.x + std::min(width - 1, (index % columns) * moduleWidth + 1),
            origin.y +
                std::min(height - 1, (index / columns) * moduleHeight + 1)
        };
    }
} // namespace Paladin
