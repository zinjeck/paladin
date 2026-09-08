#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <algorithm>
namespace Paladin
{
    inline SettlementObjectFootprint buildingInterior(
        const SettlementObjectFootprint& f,
        std::string_view type
    )
    {
        const auto* d = SettlementObjectCatalog::definition(type);
        const int band = d ? d->wallThickness : 0;
        return {
            {f.topLeft.x + band, f.topLeft.y + band},
            std::max(0, f.width - 2 * band),
            std::max(0, f.height - 2 * band)
        };
    }
    inline bool validDoorTile(
        const SettlementObjectFootprint& f,
        SettlementTilePosition p
    )
    {
        if (!f.contains(p))
        {
            return false;
        }
        const bool horizontal =
            p.x == f.topLeft.x || p.x == f.topLeft.x + f.width - 1;
        const bool vertical =
            p.y == f.topLeft.y || p.y == f.topLeft.y + f.height - 1;
        return horizontal != vertical;
    }
    inline SettlementTilePosition centerDoor(
        const SettlementObjectFootprint& f,
        int side = 2
    )
    {
        switch (side % 4)
        {
        case 0:
            return {f.topLeft.x + f.width / 2, f.topLeft.y};
        case 1:
            return {f.topLeft.x + f.width - 1, f.topLeft.y + f.height / 2};
        case 2:
            return {f.topLeft.x + f.width / 2, f.topLeft.y + f.height - 1};
        default:
            return {f.topLeft.x, f.topLeft.y + f.height / 2};
        }
    }
    inline SettlementTilePosition insideDoor(
        const SettlementObjectFootprint& f,
        SettlementTilePosition p
    )
    {
        if (p.y == f.topLeft.y)
        {
            ++p.y;
        }
        else if (p.x == f.topLeft.x + f.width - 1)
        {
            --p.x;
        }
        else if (p.y == f.topLeft.y + f.height - 1)
        {
            --p.y;
        }
        else
        {
            ++p.x;
        }
        return p;
    }
    inline void appendRoomPath(
        std::vector<SettlementTilePosition>& path,
        SettlementTilePosition from,
        SettlementTilePosition to,
        const SettlementObjectFootprint& f,
        SettlementTilePosition door
    )
    {
        const auto walk = [&](SettlementTilePosition target)
        {
            while (from != target)
            {
                if (from.x != target.x)
                {
                    from.x += from.x < target.x ? 1 : -1;
                }
                else
                {
                    from.y += from.y < target.y ? 1 : -1;
                }
                path.push_back(from);
            }
        };
        if (from == to)
        {
            return;
        }
        if (from == door)
        {
            walk(insideDoor(f, door));
        }
        if (to == door)
        {
            walk(insideDoor(f, door));
        }
        walk(to);
    }
    inline SettlementTilePosition outsideDoor(
        const SettlementObjectFootprint& f,
        SettlementTilePosition p
    )
    {
        if (p.y == f.topLeft.y)
        {
            --p.y;
        }
        else if (p.x == f.topLeft.x + f.width - 1)
        {
            ++p.x;
        }
        else if (p.y == f.topLeft.y + f.height - 1)
        {
            ++p.y;
        }
        else
        {
            --p.x;
        }
        return p;
    }
} // namespace Paladin
