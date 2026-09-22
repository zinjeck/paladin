#pragma once

#include "rendering/BuildingView.h"
#include "rendering/OutdoorGround.h"
#include "world/settlements/objects/WorkplaceCompound.h"

namespace Paladin
{
    inline void compoundWorkplace(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        std::string_view type,
        const SettlementObjectFootprint& yard,
        std::uint64_t id,
        double doorOpen = 0
    )
    {
        if (yard.width < 5 || yard.height < 5)
        {
            return;
        }
        const bool fishing = type == SettlementObjectTypes::FishingGrounds;
        const auto room = workplaceRoom(yard);
        const auto door = workplaceRoomDoor(yard);
        const auto ground = queue.size();
        outdoorGround(
            queue,
            view,
            sprites,
            fishing ? "fishing_grounds.floor" : "market.floor",
            {double(yard.topLeft.x),
             double(yard.topLeft.y),
             0,
             double(yard.width),
             double(yard.height),
             0,
             0},
            {167, 141, 114, 255},
            yard.topLeft.y,
            id,
            0
        );
        queue.setLayerFrom(ground, -2);
        const auto block = [&](double x,
                               double y,
                               double w,
                               double h,
                               RenderColor color,
                               int part)
        {
            const auto b = view.bounds({x, y, 0, w, h, 0, 1});
            if (view.visible(b))
            {
                queue.submit({b, color, y, id, 0, part});
            }
        };
        // Driven corner piles and a short sill seat the dock on the ground.
        // Props are independent of floor art and respect the usable doorway.
        for (int edge = 0; edge < 2; ++edge)
        {
            const double x = yard.topLeft.x + (edge ? yard.width - .125 : 0);
            for (int end = 0; end < 2; ++end)
            {
                const double y = yard.topLeft.y + (end ? yard.height : .25);
                if (fishing)
                {
                    block(x, y, .1875, .625, {99, 62, 75, 255}, 4);
                    block(
                        x + .0625,
                        y - .125,
                        .0625,
                        .5,
                        {189, 134, 76, 255},
                        5
                    );
                }
            }
        }
        const auto inside = workplaceStorageRoom(yard);
        if (!policy.roofsVisible)
        {
            for (int i = 0; i < 3; ++i)
            {
                sprites.placed(
                    queue,
                    view,
                    "stockpile.crate",
                    inside.topLeft.x + .5 + i * .8,
                    inside.topLeft.y + .75,
                    inside.topLeft.y + .75,
                    id,
                    40 + i,
                    .65,
                    .46
                );
            }
        }
        // A few outside stores leave a continuous route between road and door.
        const double x = yard.topLeft.x + yard.width - .75;
        const double y = yard.topLeft.y + yard.height - .45;
        for (int i = 0; i < 2; ++i)
        {
            sprites.placed(
                queue,
                view,
                "stockpile.crate",
                x - i * .8,
                y,
                y,
                id,
                50 + i,
                .65,
                .46
            );
        }
        if (!fishing)
        {
            sprites.placed(
                queue,
                view,
                "home.detail.jars",
                x,
                yard.topLeft.y + .7,
                yard.topLeft.y + .7,
                id,
                55,
                .55,
                .55
            );
        }
        modularBuilding(
            queue,
            view,
            sprites,
            policy,
            std::string(type),
            room,
            door,
            id,
            doorOpen
        );
    }
} // namespace Paladin
