#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementSimulationState.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Paladin
{
    class World;

    // Settlement is the canonical domain concept. Terms such as city, town, or
    // village may be used by presentation/UI, but they are not separate entity
    // types. For now, settlement scale is data-driven (primarily population).
    class Settlement
    {
    public:
        Settlement(SettlementId id, WorldTilePosition position) noexcept
            : id_(id), position_(position)
        {
        }

        Settlement(
            SettlementId id,
            WorldTilePosition position,
            std::string name,
            RealmId ownerRealmId,
            CultureId primaryCultureId,
            const SettlementFoundationProfile& foundationProfile
        )
            : id_(id), position_(position), name_(std::move(name)),
              ownerRealmId_(ownerRealmId), primaryCultureId_(primaryCultureId)
        {
            if (!simulationState_.bootstrap(foundationProfile))
            {
                throw std::invalid_argument(
                    "Invalid settlement foundation profile."
                );
            }
        }

        [[nodiscard]]
        SettlementId id() const noexcept
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

        [[nodiscard]]
        std::uint64_t population() const noexcept
        {
            return simulationState_.population().residents();
        }

        [[nodiscard]]
        SettlementSimulationState& simulationState() noexcept
        {
            return simulationState_;
        }

        [[nodiscard]]
        const SettlementSimulationState& simulationState() const noexcept
        {
            return simulationState_;
        }

    private:
        friend class World;

        void setPosition(WorldTilePosition position) noexcept
        {
            position_ = position;
        }

        void setName(std::string name)
        {
            name_ = std::move(name);
        }

        void setOwnerRealm(RealmId realmId) noexcept
        {
            ownerRealmId_ = realmId;
        }

        SettlementId id_;

        WorldTilePosition position_;

        std::string name_;

        // Invalid ID means independent / currently unowned.
        RealmId ownerRealmId_;
        CultureId primaryCultureId_;
        SettlementSimulationState simulationState_;
    };
} // namespace Paladin
