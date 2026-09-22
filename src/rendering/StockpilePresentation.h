#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/WorkYardFoundation.h"
#include "rendering/OutdoorGround.h"
#include "rendering/StockpileShelter.h"
#include "world/settlements/SettlementMap.h"
#include <array>
#include <string_view>
namespace Paladin
{
    inline bool stockpilePresentation(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const SettlementMap& map,
        const CompletedSettlementObject& object,
        std::uint64_t id
    )
    {
        if (!sprites.find("roof.thatch.full"))
        {
            return false;
        }
        const auto& f = object.footprint;
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        const auto floorStart = q.size();
        outdoorGround(
            q,
            p,
            sprites,
            "stockpile.floor",
            {x, y, 0, w, h, 0, 0},
            {},
            y,
            id,
            0,
            false
        );
        q.setLayerFrom(floorStart, -2);
        workYardFoundation(q, p, map, object, id, true);
        const auto shelter = StockpileShelterLayout::fit(f);
        stockpileShelter(q, p, sprites, shelter, id);
        // Low raised rails outline the yard, with real footings and an open
        // loading side. They do not turn the drawable stockpile into a room.
        constexpr double fenceHeight = .375;
        const double backFoot = y + .1875, frontFoot = y + h - .1875;
        for (const double xx : {x + .0625, x + w - .1875})
        {
            q.submit({p.bounds({xx,backFoot,0,.1875,frontFoot-backFoot,0,0}),
                      {57,43,60,65},y+h,id,-1,1});
            q.submit({p.bounds({xx,backFoot-fenceHeight,0,.125,frontFoot-backFoot,0,0}),
                      {122,80,56,255},y+h,id,0,1});
            q.submit({p.bounds({xx,backFoot-fenceHeight,0,.0625,frontFoot-backFoot,0,0}),
                      {193,139,90,255},y+h,id,0,2});
            for (const double yy : {backFoot,frontFoot})
            {
                q.submit({p.bounds({xx-.0625,yy-.0625,0,.25,.1875,0,0}),
                          {73,53,47,255},yy,id,0,3});
                q.submit({p.bounds({xx,yy,0,.1875,fenceHeight,0,1}),
                          {122,80,56,255},yy,id,0,4});
                q.submit({p.bounds({xx,yy-fenceHeight,0,.1875,.0625,0,0}),
                          {215,200,162,255},yy,id,0,5});
                q.submit({p.bounds({xx,yy-fenceHeight,0,.0625,fenceHeight,0,0}),
                          {193,139,90,255},yy,id,0,6});
            }
        }
        const auto* inventory =
            map.logistics.inventory(map.logistics.forObject(object.id));
        if (!inventory)
        {
            return true;
        }
        const double storageTop = std::min(y + h - .625, shelter.front() + .25);
        const int columns = std::max(1, int((w - .5) / .8)),
                  rows = std::max(1, int((y + h - storageTop) / .625));
        const int capacity = std::min(48, columns * rows);
        // Show distinct stored goods before adding second/third stacks. A small
        // mixed stockpile must not look like it contains only its first resource.
        std::array<std::string_view,48> stacks{};
        int stackCount = 0;
        for (int round = 0; round < 4 && stackCount < capacity; ++round)
        {
            for (const auto& goods : inventory->goods)
            {
                if (stackCount >= capacity) { break; }
                if (goods.amount > round * 20) { stacks[stackCount++] = goods.resource; }
            }
        }
        for (int slot = 0; slot < stackCount; ++slot)
        {
            auto sprite = "resource." + std::string(stacks[slot]);
            if (!sprites.find(sprite))
            {
                sprite = "resource.pile";
            }
            {
                const double xx = std::round((x + .3125 +
                    ((slot % columns) + .5) * (w - .625) / columns) * 16) / 16;
                const double yy = storageTop + .5 + (slot / columns) * .625;
                const double baseY = std::min(y + h - .1875, yy);
                sprites.placed(
                    q,
                    p,
                    "stockpile.crate",
                    xx,
                    baseY,
                    baseY + .1,
                    id,
                    10 + slot * 2,
                    .625,
                    .4375
                );
                sprites.placed(
                    q,
                    p,
                    sprite,
                    xx,
                    baseY - .125,
                    baseY + .1,
                    id,
                    11 + slot * 2,
                    .4375,
                    .25
                );
            }
        }
        return true;
    }
} // namespace Paladin
