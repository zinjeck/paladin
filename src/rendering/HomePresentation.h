#pragma once
#include "rendering/BuildingView.h"
#include "world/settlements/SettlementHomeBeds.h"

namespace Paladin
{
    // Cosmetic choices depend only on a home's persistent identity, never on
    // zoom.
    inline void homeDetails(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const CompletedSettlementObject& home,
        std::uint64_t id,
        unsigned doubleRows = 0
    )
    {
        if (home.objectTypeId != SettlementObjectTypes::House ||
            p.tilePixels < StaticDetailPixels)
        {
            return;
        }
        const auto& f = home.footprint;
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        std::uint64_t seed = home.id.value() * 0x9E3779B185EBCA87ull;
        seed ^= seed >> 29;
        const auto start = queue.size();
        if (policy.roofsVisible)
        {
            const int facing =
                buildingView(f, home.door, policy.viewAzimuthDegrees);
            // Attach details to the visible wall plane. No ground containers
            // are overlaid on the facade. The door's center remains clear.
            const double wallHeight =
                sprites.objectStyle(home.objectTypeId).height;
            const double bottom = y + h - .12;
            const double depth = y + h + .02;
            const bool left = (seed >> 3) % 2;
            const double mountX = x + w * (left ? .19 : .81);
            sprites.placed(
                queue,
                p,
                "home.detail.shutters",
                mountX,
                bottom - .20,
                depth,
                id,
                24,
                .48,
                std::min(.38, wallHeight * .48)
            );
            if ((seed >> 5) % 3 != 0)
            {
                sprites.placed(
                    queue,
                    p,
                    "home.detail.hide",
                    x + w * (left ? .81 : .19),
                    bottom,
                    depth,
                    id,
                    25,
                    .32,
                    std::min(.48, wallHeight * .65)
                );
            }
            // Pegs, lintel and shallow timber sill share the wall's depth.
            const auto timber = [&](double xx,
                                    double yy,
                                    double ww,
                                    double hh,
                                    RenderColor color,
                                    int part)
            {
                queue.submit(
                    {p.bounds({xx, yy, 0, ww, hh, 0, 0}),
                     color,
                     depth,
                     id,
                     0,
                     part}
                );
            };
            timber(
                mountX - .27,
                bottom - .18,
                .54,
                .0625,
                {0xBD, 0x86, 0x4C, 255},
                26
            );
            timber(
                mountX - .23,
                bottom - .64,
                .46,
                .0625,
                {0x63, 0x3E, 0x4B, 255},
                27
            );
            if (facing == 2)
            {
                timber(
                    x + w * .43,
                    y + h - wallHeight * .82,
                    w * .14,
                    .0625,
                    {0xBD, 0x86, 0x4C, 255},
                    28
                );
            }
        }
        else
        {
            // The center aisle is clear. Rugs and storage sit between the
            // corner beds.
            constexpr RenderColor cloth[] = {
                {0xA6, 0x35, 0x45, 255},
                {0x3F, 0x5F, 0x9A, 255},
                {0x6F, 0x4A, 0x7E, 255},
                {0x4F, 0x8C, 0x7A, 255},
                {0xB7, 0x83, 0x50, 255},
                {0xD9, 0xC7, 0x9F, 255},
                {0x87, 0x4D, 0x50, 255},
                {0x63, 0xBF, 0xC3, 255}
            };
            queue.submit(
                {p.bounds({x + w * .5, y + h * .5, 0, .7, .7, .5, .5}),
                 cloth[seed % 8],
                 y,
                 id,
                 -1,
                 0}
            );
            const auto rugColor = cloth[(seed + 3) % 8];
            for (int stripe = 0; stripe < 4; ++stripe)
            {
                queue.submit(
                    {p.bounds(
                         {x + w * .5 - .3,
                          y + h * .5 - .27 + stripe * .16,
                          0,
                          .6,
                          .035,
                          0,
                          0}
                     ),
                     rugColor,
                     y,
                     id,
                     -1,
                     1}
                );
            }
            for (int slot = 0; slot < 4; ++slot)
            {
                const bool shared = (doubleRows & (1u << (slot / 2))) != 0;
                if (shared && slot % 2)
                {
                    continue;
                }
                const auto at = homeBedPosition(home, slot);
                const auto first = queue.size();
                sprites.placed(
                    queue,
                    p,
                    "home.bed." + std::to_string((seed + slot * 3) % 8),
                    shared ? x + w * .5 : at.x + .5,
                    at.y + .96,
                    at.y + .1,
                    id,
                    3,
                    shared ? 1.45 : .72,
                    .92
                );
                // Beds are ground furniture; citizens and wall caps stay above
                // them.
                queue.setLayerFrom(first, -1);
            }
            const double shelfX = (seed >> 6) % 2 ? x + .5 : x + w - .5;
            const auto first = queue.size();
            sprites.placed(
                queue,
                p,
                (seed >> 7) % 2 ? "home.detail.jars" : "home.detail.basket",
                shelfX,
                y + h * .5 + .2,
                y + h * .5,
                id,
                4,
                .4,
                .45
            );
            queue.setLayerFrom(first, -1);
        }
        queue.setOpacityFrom(
            start,
            detailBlend(p.tilePixels, StaticDetailPixels, 32)
        );
    }
} // namespace Paladin
