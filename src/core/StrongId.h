#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>

namespace Paladin
{
    template<typename Tag>
    class StrongId
    {
    public:
        using ValueType = std::uint64_t;

        constexpr StrongId() noexcept = default;

        explicit constexpr StrongId(ValueType value) noexcept
            : value_(value)
        {
        }

        [[nodiscard]]
        constexpr ValueType value() const noexcept
        {
            return value_;
        }

        [[nodiscard]]
        constexpr bool isValid() const noexcept
        {
            return value_ != 0;
        }

        explicit constexpr operator bool() const noexcept
        {
            return isValid();
        }

        friend constexpr bool operator==(
            StrongId,
            StrongId
        ) noexcept = default;

        friend constexpr auto operator<=>(
            StrongId,
            StrongId
        ) noexcept = default;

    private:
        // Zero is permanently reserved as "no ID".
        ValueType value_ = 0;
    };


    struct StrongIdHash
    {
        template<typename Tag>
        std::size_t operator()(
            StrongId<Tag> id
        ) const noexcept
        {
            return std::hash<std::uint64_t>{}(id.value());
        }
    };


    template<typename IdType>
    class IdGenerator
    {
    public:
        [[nodiscard]]
        IdType generate()
        {
            if (nextValue_ == 0)
            {
                throw std::overflow_error(
                    "Paladin entity ID space exhausted."
                );
            }

            const auto value = nextValue_;

            if (
                nextValue_ ==
                std::numeric_limits<
                    typename IdType::ValueType
                >::max()
            )
            {
                nextValue_ = 0;
            }
            else
            {
                ++nextValue_;
            }

            return IdType{ value };
        }

    private:
        typename IdType::ValueType nextValue_ = 1;
    };


    // Distinct tags make these IDs incompatible at compile time.

    struct SettlementIdTag;
    struct PolityIdTag;
    struct ArmyIdTag;
    struct CitizenIdTag;
    struct CultureIdTag;
    struct SettlementObjectIdTag;
    struct ConstructionSiteIdTag;
    struct SettlementCommandIdTag;
    struct WorkplaceIdTag;

    using SettlementId = StrongId<SettlementIdTag>;
    using PolityId = StrongId<PolityIdTag>;
    using ArmyId = StrongId<ArmyIdTag>;
    using CitizenId = StrongId<CitizenIdTag>;
    using CultureId = StrongId<CultureIdTag>;
    using SettlementObjectId = StrongId<SettlementObjectIdTag>;
    using ConstructionSiteId = StrongId<ConstructionSiteIdTag>;
    using WorkplaceId = StrongId<WorkplaceIdTag>;
    using SettlementCommandId = StrongId<SettlementCommandIdTag>;
}
