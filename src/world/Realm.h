#pragma once

#include "core/StrongId.h"
#include "world/FoundingIdentity.h"
#include "world/settlements/SettlementCommerce.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace Paladin
{
    class Realm
    {
    public:
        std::shared_ptr<Treasury> treasury = std::make_shared<Treasury>();
        explicit Realm(RealmId id) noexcept : id_(id) {}

        [[nodiscard]]
        RealmId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        CultureId primaryCultureId() const noexcept
        {
            return primaryCultureId_;
        }

        [[nodiscard]]
        SettlementId capitalSettlementId() const noexcept
        {
            return capitalSettlementId_;
        }

        [[nodiscard]]
        MapColor mapColor() const noexcept
        {
            return mapColor_;
        }

        [[nodiscard]]
        std::string_view name() const noexcept
        {
            return name_;
        }

        [[nodiscard]]
        std::string_view startingOriginId() const noexcept
        {
            return startingOriginId_;
        }

        [[nodiscard]]
        const RealmFlag& flag() const noexcept
        {
            return flag_;
        }

        int workDayHours() const noexcept
        {
            return workDayHours_;
        }
        void setWorkDayHours(int hours) noexcept
        {
            workDayHours_ = std::clamp(hours, 0, 14);
        }

    private:
        int workDayHours_ = 12;
        friend class World;

        void establishCapital(
            SettlementId settlementId,
            CultureId cultureId,
            MapColor mapColor,
            std::string realmName,
            std::string startingOriginId,
            RealmFlag flag
        )
        {
            capitalSettlementId_ = settlementId;
            primaryCultureId_ = cultureId;
            mapColor_ = mapColor;
            name_ = std::move(realmName);
            startingOriginId_ = std::move(startingOriginId);
            flag_ = std::move(flag);
        }

        void editIdentity(
            MapColor mapColor,
            std::string realmName,
            std::string startingOriginId,
            RealmFlag flag
        )
        {
            mapColor_ = mapColor;
            name_ = std::move(realmName);
            startingOriginId_ = std::move(startingOriginId);
            flag_ = std::move(flag);
        }

        RealmId id_;
        CultureId primaryCultureId_;
        SettlementId capitalSettlementId_;
        MapColor mapColor_;
        std::string name_;
        std::string startingOriginId_;
        RealmFlag flag_;
    };
} // namespace Paladin
