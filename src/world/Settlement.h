#pragma once

#include "core/StrongId.h"
#include "world/WorldPosition.h"

namespace Paladin
{
    class World;

    class Settlement
    {
    public:
        Settlement(
            SettlementId id,
            WorldPosition position
        ) noexcept
            : id_(id),
              position_(position)
        {
        }

        [[nodiscard]]
        SettlementId id() const noexcept
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

        SettlementId id_;

        WorldPosition position_;

        // Invalid ID means independent / currently unowned.
        PolityId ownerPolityId_;
    };
}