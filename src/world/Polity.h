#pragma once

#include "core/StrongId.h"
#include "world/FoundingIdentity.h"

#include <string>
#include <string_view>
#include <utility>

namespace Paladin
{
    class Polity
    {
    public:
        explicit Polity(
            PolityId id
        ) noexcept
            : id_(id)
        {
        }

        [[nodiscard]]
        PolityId id() const noexcept
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

    private:
        friend class World;

        void establishCapital(
            SettlementId settlementId,
            CultureId cultureId,
            MapColor mapColor,
            std::string polityName,
            std::string startingOriginId
        ) noexcept
        {
            capitalSettlementId_ = settlementId;
            primaryCultureId_ = cultureId;
            mapColor_ = mapColor;
            name_ = std::move(polityName);
            startingOriginId_ = std::move(startingOriginId);
        }

        PolityId id_;
        CultureId primaryCultureId_;
        SettlementId capitalSettlementId_;
        MapColor mapColor_;
        std::string name_;
        std::string startingOriginId_;
    };
}
