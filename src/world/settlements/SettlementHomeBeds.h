#pragma once
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <array>
#include <span>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    inline SettlementTilePosition homeBedPosition(
        const CompletedSettlementObject& home,
        int slot
    )
    {
        const auto f = buildingInterior(home.footprint, home.objectTypeId);
        // Four corner sleeping spaces leave the entrance and central aisle
        // clear.
        return {
            f.topLeft.x + (slot % 2 ? f.width - 1 : 0),
            f.topLeft.y + (slot / 2 ? f.height - 1 : 0)
        };
    }
    inline void assignHomeBeds(
        const SettlementMap& map,
        std::span<SettlementCitizen> people
    )
    {
        std::unordered_map<SettlementObjectId, unsigned, StrongIdHash> occupied;
        for (auto& c : people)
        {
            c.doubleBed = false;
            c.bedVisualOffsetX = 0;
            const auto* home = map.objectState().completedObject(c.homeId);
            if (c.health <= 0 || !home ||
                home->objectTypeId != SettlementObjectTypes::House)
            {
                c.bedHomeId = {};
                c.bedSlot = -1;
                continue;
            }
            auto& mask = occupied[c.homeId];
            if (c.bedHomeId != c.homeId || c.bedSlot < 0 || c.bedSlot >= 4 ||
                (mask & (1u << c.bedSlot)))
            {
                c.bedHomeId = c.homeId;
                c.bedSlot = -1;
            }
            else
            {
                mask |= 1u << c.bedSlot;
            }
        }
        for (auto& c : people)
        {
            if (!c.bedHomeId || c.bedSlot >= 0)
            {
                continue;
            }
            auto& mask = occupied[c.bedHomeId];
            for (int slot = 0; slot < 4; ++slot)
            {
                if (!(mask & (1u << slot)))
                {
                    c.bedSlot = slot;
                    mask |= 1u << slot;
                    break;
                }
            }
        }
        // Pair co-resident spouses into rows. Keep navigation destinations
        // distinct; presentation brings their sleeping positions together.
        std::unordered_map<
            SettlementObjectId,
            std::vector<SettlementCitizen*>,
            StrongIdHash>
            households;
        for (auto& c : people)
        {
            if (c.bedHomeId && c.bedSlot >= 0)
            {
                households[c.bedHomeId].push_back(&c);
            }
        }
        for (auto& [homeId, residents] : households)
        {
            std::stable_sort(
                residents.begin(),
                residents.end(),
                [](auto* a, auto* b) { return a->id.value() < b->id.value(); }
            );
            unsigned paired = 0;
            for (auto* a : residents)
            {
                if (!a->spouseId || a->doubleBed || a->child)
                {
                    continue;
                }
                auto partner = std::find_if(
                    residents.begin(),
                    residents.end(),
                    [&](auto* b)
                    {
                        return b != a && b->id == a->spouseId &&
                               b->spouseId == a->id && !b->child &&
                               !b->doubleBed;
                    }
                );
                if (partner == residents.end())
                {
                    continue;
                }
                auto* b = *partner;
                int row = a->bedSlot / 2;
                if (paired & (3u << (row * 2)))
                {
                    row = 1 - row;
                }
                a->bedSlot = row * 2;
                b->bedSlot = row * 2 + 1;
                a->doubleBed = b->doubleBed = true;
                paired |= 3u << (row * 2);
            }
            auto mask = paired;
            // Preserve unpaired residents' previous slots where possible.
            for (auto* c : residents)
            {
                if (!c->doubleBed)
                {
                    if (mask & (1u << c->bedSlot))
                    {
                        c->bedSlot = -1;
                    }
                    else
                    {
                        mask |= 1u << c->bedSlot;
                    }
                }
            }
            for (auto* c : residents)
            {
                if (c->bedSlot < 0)
                {
                    for (int slot = 0; slot < 4; ++slot)
                    {
                        if (!(mask & (1u << slot)))
                        {
                            c->bedSlot = slot;
                            mask |= 1u << slot;
                            break;
                        }
                    }
                }
                if (c->doubleBed)
                {
                    const auto* home =
                        map.objectState().completedObject(homeId);
                    const double half = (home->footprint.width - 1) * .5;
                    c->bedVisualOffsetX =
                        c->bedSlot % 2 ? .26 - half : half - .26;
                }
            }
        }
    }
} // namespace Paladin
