#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/FramedWall.h"
#include "rendering/SceneDetail.h"
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
    // Shared families are selected by the object catalog. Legacy directional
    // exports remain supported by tribalBuilding when a family is unavailable.
    bool modularBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen
    );

    bool tribalBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen = 0
    );
} // namespace Paladin
