#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementDoor.h"
#include <cmath>

namespace Paladin
{
    // World-space entrance heading is independent of the camera. Additional
    // angle samples can be added here when the camera gains continuous yaw.
    inline double buildingHeading(
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door
    )
    {
        if (!door)
        {
            return 180;
        }
        if (door->y == f.topLeft.y)
        {
            return 0;
        }
        if (door->x == f.topLeft.x + f.width - 1)
        {
            return 90;
        }
        if (door->y == f.topLeft.y + f.height - 1)
        {
            return 180;
        }
        return 270;
    }
    inline int buildingView(
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        double cameraHeading = 0
    )
    {
        const auto angle =
            std::fmod(buildingHeading(f, door) - cameraHeading + 720., 360.);
        return int(std::floor((angle + 45) / 90)) % 4;
    }
    inline bool tribalBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen = 0
    )
    {
        if (!policy.roofsVisible || !sprites.find(type + ".wall.front"))
        {
            return false;
        }
        const auto& style = sprites.objectStyle(type);
        const int facing = buildingView(f, door, policy.viewAzimuthDegrees);
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        const auto roofName =
            type + ((h > w * 1.4 || (std::abs(w - h) < .5 && facing % 2))
                        ? ".roof.full.side"
                        : ".roof.full");
        const auto* roof = sprites.find(roofName);
        if (!roof)
        {
            return false;
        }
        // One fitted roof surface for the entire blueprint, including
        // extensions.
        sprites.placed(
            queue,
            p,
            roofName,
            x - .22,
            y,
            y + h,
            id,
            10,
            w + .44,
            h + .08
        );
        const auto face = type + (facing == 2   ? ".wall.front"
                                  : facing == 0 ? ".wall.back"
                                                : ".wall.side");
        sprites.placed(
            queue,
            p,
            face,
            x,
            y + h,
            y + h,
            id,
            12,
            w,
            style.height,
            facing == 2 ? doorOpen : 0
        );
        if (facing % 2)
        {
            // The entrance appears on its actual lateral edge. Its narrow
            // projected face also distinguishes the two side views.
            const double yy = door ? door->y + 1 : y + h * .65;
            const double xx = facing == 1 ? x + w - .12 : x - .28;
            sprites.placed(
                queue,
                p,
                type + ".wall.front",
                xx,
                yy,
                y + h + .01,
                id,
                13,
                .4,
                style.height,
                doorOpen
            );
        }
        return true;
    }
} // namespace Paladin
