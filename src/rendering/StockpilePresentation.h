#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
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
        if (!sprites.find("stockpile.shelter.roof"))
        {
            return false;
        }
        const auto& f = object.footprint;
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        const auto floorStart = q.size();
        sprites.surface(
            q,
            p,
            "stockpile.floor",
            {x, y, 0, w, h, 0, 0},
            {},
            y,
            id,
            0,
            false
        );
        q.setLayerFrom(floorStart, -2);
        const double shedW = std::min(2.5, std::max(1., w - .3)),
                     shedH = std::min(1.2, h * .4);
        if (sprites.shadowsEnabled())
        {
            // The shelter shades its platform, not the inventory drawn above.
            q.submit(
                {p.bounds({x + .16, y + .20, 0, shedW, shedH, 0, 0}),
                 {57, 43, 60, 46},
                 y,
                 id,
                 -1,
                 0}
            );
            for (const double xx : {x + .10, x + w - .10})
            {
                for (const double yy : {y + .15, y + h - .1})
                {
                    q.submit(
                        {p.bounds({xx, yy, 0, .28, .125, 0, 0}),
                         {57, 43, 60, 64},
                         y,
                         id,
                         -1,
                         0}
                    );
                }
            }
        }
        // A modest open storage shelter, with the goods visible in its yard.
        sprites.placed(
            q,
            p,
            "stockpile.shelter.wall",
            x + .15,
            y + shedH,
            y + shedH,
            id,
            2,
            shedW,
            .6
        );
        sprites.placed(
            q,
            p,
            "stockpile.shelter.roof",
            x + .08,
            y - .38,
            y + shedH,
            id,
            3,
            shedW + .14,
            shedH + .2
        );
        // Low timber boundaries and corner posts identify the storage yard
        // without hiding the inventory behind another full roof.
        for (int edge = 0; edge < 2; ++edge)
        {
            const double xx = edge ? x + w - .10 : x;
            q.submit(
                {p.bounds({xx, y, 0, .10, h, 0, 0}),
                 {0x63, 0x3E, 0x4B, 255},
                 y + h,
                 id,
                 0,
                 4}
            );
            for (const double yy : {y + .15, y + h})
            {
                q.submit(
                    {p.bounds({xx, yy, 0, .14, .65, 0, 1}),
                     {0x74, 0x51, 0x3F, 255},
                     yy,
                     id,
                     0,
                     5}
                );
            }
        }
        const auto* inventory =
            map.logistics.inventory(map.logistics.forObject(object.id));
        if (!inventory)
        {
            return true;
        }
        const int columns = std::max(1, int(w / .8)),
                  rows = std::max(1, int((h - shedH) / .7));
        const int capacity = std::min(48, columns * rows);
        int slot = 0;
        for (const auto& goods : inventory->goods)
        {
            if (goods.amount <= 0 || slot >= capacity)
            {
                continue;
            }
            auto sprite = "resource." + goods.resource;
            if (!sprites.find(sprite))
            {
                sprite = "resource.pile";
            }
            const int stacks =
                std::min({4, 1 + (goods.amount - 1) / 20, capacity - slot});
            for (int i = 0; i < stacks; ++i, ++slot)
            {
                const double xx = x + .4 + (slot % columns) * w / columns,
                             yy = y + shedH + .45 + (slot / columns) * .7;
                const double baseY = std::min(y + h - .08, yy);
                sprites.placed(
                    q,
                    p,
                    "stockpile.crate",
                    xx,
                    baseY,
                    baseY + .1,
                    id,
                    10 + slot * 2,
                    .66,
                    .46
                );
                sprites.placed(
                    q,
                    p,
                    sprite,
                    xx,
                    baseY - .10,
                    baseY + .1,
                    id,
                    11 + slot * 2,
                    .43,
                    .25
                );
            }
        }
        return true;
    }
} // namespace Paladin
