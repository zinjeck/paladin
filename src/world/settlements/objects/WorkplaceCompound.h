#pragma once

#include "world/settlements/objects/SettlementDoor.h"

namespace Paladin
{
    inline bool workplaceCompound(std::string_view type)
    {
        return type == SettlementObjectTypes::TradeDepot ||
               type == SettlementObjectTypes::FishingGrounds;
    }
    // The occupied yard and enclosed room are distinct. Rendering, collision,
    // storage and previews all use this plan; interior props remain separate.
    inline SettlementObjectFootprint workplaceRoom(
        const SettlementObjectFootprint& yard
    )
    {
        return {
            yard.topLeft,
            std::min(5, yard.width),
            std::min(5, yard.height)
        };
    }
    inline SettlementTilePosition workplaceRoomDoor(
        const SettlementObjectFootprint& yard
    )
    {
        return centerDoor(
            workplaceRoom(yard),
            yard.width >= yard.height ? 1 : 2
        );
    }
    inline SettlementObjectFootprint workplaceStorageRoom(
        const SettlementObjectFootprint& yard
    )
    {
        auto room = workplaceRoom(yard);
        ++room.topLeft.x;
        ++room.topLeft.y;
        room.width = std::max(1, room.width - 2);
        room.height = std::max(1, room.height - 2);
        return room;
    }
} // namespace Paladin
