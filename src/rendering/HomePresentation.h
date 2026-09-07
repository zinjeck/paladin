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
            // Keep the door and existing windows readable; use the blank wall
            // margins.
            constexpr const char* extras[] =
                {"basket", "hide", "jars", "woodrack"};
            sprites.placed(
                queue,
                p,
                std::string("home.detail.") + extras[seed % 4],
                x + w * .12,
                y + h - .02,
                y + h + .02,
                id,
                24,
                .46,
                .53
            );
            if (facing != 2 || (seed >> 4) % 2)
            {
                sprites.placed(
                    queue,
                    p,
                    "home.detail.jars",
                    x + w * .86,
                    y + h + .05,
                    y + h + .03,
                    id,
                    25,
                    .45,
                    .43
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
                if (shared && slot % 2) continue;
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
