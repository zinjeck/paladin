#include "world/World.h"

#include <vector>

namespace Paladin
{
    WorldRoadId World::createWorldRoad(
        std::span<const WorldTilePosition> points,
        RealmId ownerRealmId
    )
    {
        if (points.size() < 2 ||
            (ownerRealmId && !realms_.contains(ownerRealmId)))
        {
            return {};
        }
        for (const WorldTilePosition point : points)
        {
            if (!grid_.isValidPosition(point))
            {
                return {};
            }
        }
        return worldRoads_.create(
            std::vector<WorldTilePosition>(points.begin(), points.end()),
            ownerRealmId
        );
    }

    WorldRoad* World::worldRoad(WorldRoadId id) noexcept
    {
        return worldRoads_.find(id);
    }

    const WorldRoad* World::worldRoad(WorldRoadId id) const noexcept
    {
        return worldRoads_.find(id);
    }

    std::span<const Army> World::armies() const noexcept
    {
        return armies_.entities();
    }

    std::span<Army> World::armies() noexcept
    {
        return armies_.entities();
    }

    std::span<const WorldRoad> World::worldRoads() const noexcept
    {
        return worldRoads_.entities();
    }

    std::span<WorldRoad> World::worldRoads() noexcept
    {
        return worldRoads_.entities();
    }

    bool World::assignWorldRoadToRealm(
        WorldRoadId roadId,
        RealmId realmId
    ) noexcept
    {
        WorldRoad* road = worldRoads_.find(roadId);
        if (!road || !realms_.contains(realmId))
        {
            return false;
        }
        road->setOwnerRealm(realmId);
        return true;
    }

    bool World::makeWorldRoadIndependent(WorldRoadId roadId) noexcept
    {
        WorldRoad* road = worldRoads_.find(roadId);
        if (!road)
        {
            return false;
        }
        road->setOwnerRealm({});
        return true;
    }

    std::size_t World::worldRoadCount() const noexcept
    {
        return worldRoads_.size();
    }
} // namespace Paladin
