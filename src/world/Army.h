#pragma once

#include "core/StrongId.h"
#include "world/WorldPosition.h"

namespace Paladin
{
    class World;

    class Army
    {
    public:
        Army(
            ArmyId id,
            WorldPosition position
        ) noexcept
            : id_(id),
              position_(position)
        {
        }

        [[nodiscard]]
        ArmyId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        WorldPosition position() const noexcept
        {
            return position_;
        }

        [[nodiscard]]
        PolityId ownerPolityId() const noexcept
        {
            return ownerPolityId_;
        }

        [[nodiscard]]
        bool hasOwnerPolity() const noexcept
        {
            return ownerPolityId_.isValid();
        }

    private:
        friend class World;

        void setPosition(
            WorldPosition position
        ) noexcept
        {
            position_ = position;
        }

        void setOwnerPolity(
            PolityId polityId
        ) noexcept
        {
            ownerPolityId_ = polityId;
        }

        ArmyId id_;

        WorldPosition position_;

        PolityId ownerPolityId_;
    };
}