#pragma once
#include "world/settlements/objects/SettlementObjectState.h"
namespace Paladin
{
inline bool validDoorTile(
    const SettlementObjectFootprint& f,
    SettlementTilePosition p
)
{
    if (!f.contains(p))
        return false;
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
inline SettlementTilePosition outsideDoor(
    const SettlementObjectFootprint& f,
    SettlementTilePosition p
)
{
    if (p.y == f.topLeft.y)
        --p.y;
    else if (p.x == f.topLeft.x + f.width - 1)
        ++p.x;
    else if (p.y == f.topLeft.y + f.height - 1)
        ++p.y;
    else
        --p.x;
    return p;
}
} // namespace Paladin
