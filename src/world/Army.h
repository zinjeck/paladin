#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <vector>

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

        const std::string& name() const noexcept { return name_; }
        SettlementId homeSettlementId() const noexcept { return home_; }
        std::span<const SoldierId> soldiers() const noexcept { return soldiers_; }
        std::size_t soldierCount() const noexcept { return soldiers_.size(); }
        int rations() const noexcept { return rations_; }
        bool moving() const noexcept { return routeIndex_ < route_.size(); }
        bool facingNorth() const noexcept
        { return moving() && route_[routeIndex_].y < position_.y; }
        double marchDistance() const noexcept
        { return double(routeIndex_) + (moving() ? stepMinutes_ / minutesPerTile_ : 0.0); }
        WorldTilePosition destination() const noexcept
        { return moving() ? route_.back() : position_; }
        double visualX() const noexcept
        {
            if (!moving()) return position_.x;
            double dx = route_[routeIndex_].x - position_.x;
            if (wrapWidth_ > 0 && std::abs(dx) > wrapWidth_ * .5)
                dx += dx > 0 ? -wrapWidth_ : wrapWidth_;
            return position_.x + dx * std::clamp(stepMinutes_ / minutesPerTile_, 0.0, 1.0);
        }
        double visualY() const noexcept
        {
            return moving() ? position_.y + (route_[routeIndex_].y - position_.y) *
                std::clamp(stepMinutes_ / minutesPerTile_, 0.0, 1.0) : position_.y;
        }
    private:
        friend class MilitarySystem;
        std::string name_ = "Unit";
        SettlementId home_;
        std::vector<SoldierId> soldiers_;
        std::vector<WorldTilePosition> route_;
        std::size_t routeIndex_ = 0;
        double stepMinutes_ = 0;
        double minutesPerTile_ = 30;
        int wrapWidth_ = 0;
        int rations_ = 0;
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