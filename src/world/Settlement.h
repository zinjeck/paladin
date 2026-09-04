#pragma once

#include "core/StrongId.h"
#include "world/WorldPosition.h"

#include <string>
#include <string_view>
#include <utility>

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

        Settlement(
            SettlementId id,
            WorldPosition position,
            std::string name,
            PolityId ownerPolityId,
            CultureId primaryCultureId
        )
            : id_(id),
              position_(position),
              name_(std::move(name)),
              ownerPolityId_(ownerPolityId),
              primaryCultureId_(primaryCultureId)
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

        [[nodiscard]]
        std::string_view name() const noexcept
        {
            return name_;
        }

        [[nodiscard]]
        CultureId primaryCultureId() const noexcept
        {
            return primaryCultureId_;
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

        std::string name_;

        // Invalid ID means independent / currently unowned.
        PolityId ownerPolityId_;
        CultureId primaryCultureId_;
    };
}
