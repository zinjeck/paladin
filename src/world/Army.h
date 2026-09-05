#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

namespace Paladin
{
    class World;

    class Army
    {
    public:
        Army(ArmyId id, WorldTilePosition position) noexcept
            : id_(id), position_(position)
        {
        }

        [[nodiscard]]
        ArmyId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        WorldTilePosition position() const noexcept
        {
            return position_;
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

        void setPosition(WorldTilePosition position) noexcept
        {
            position_ = position;
        }

        void setOwnerRealm(RealmId realmId) noexcept
        {
            ownerRealmId_ = realmId;
        }

        ArmyId id_;

        WorldTilePosition position_;

        RealmId ownerRealmId_;
    };
} // namespace Paladin