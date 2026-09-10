#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

#include <span>
#include <utility>
#include <vector>

namespace Paladin
{
    class World;

    // Strategic roads are world entities, not settlement-local road objects.
    // They intentionally contain only durable world state; movement bonuses,
    // construction and authored presentation can layer on later without
    // changing their identity.
    class WorldRoad
    {
    public:
        WorldRoad(
            WorldRoadId id,
            std::vector<WorldTilePosition> points,
            RealmId ownerRealmId = {}
        ) noexcept
            : id_(id),
              points_(std::move(points)),
              ownerRealmId_(ownerRealmId)
        {
        }

        [[nodiscard]]
        WorldRoadId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        std::span<const WorldTilePosition> points() const noexcept
        {
            return points_;
        }

        [[nodiscard]]
        RealmId ownerRealmId() const noexcept
        {
            return ownerRealmId_;
        }

        [[nodiscard]]
        bool hasOwnerRealm() const noexcept
        {
            return ownerRealmId_.isValid();
        }

    private:
        friend class World;

        void setOwnerRealm(RealmId realmId) noexcept
        {
            ownerRealmId_ = realmId;
        }

        WorldRoadId id_;
        std::vector<WorldTilePosition> points_;
        RealmId ownerRealmId_;
    };
} // namespace Paladin
