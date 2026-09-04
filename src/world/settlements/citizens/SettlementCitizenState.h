#pragma once

#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Paladin
{
    class SettlementMap;

    enum class CitizenSex : std::uint8_t
    {
        Male,
        Female
    };

    enum class CitizenActivity : std::uint8_t
    {
        Idle,
        AssignedToCommand
    };

    struct SettlementCitizen
    {
        CitizenId id;
        std::string name;
        CitizenSex sex = CitizenSex::Male;
        SettlementTilePosition tilePosition{-1, -1};
        CitizenActivity activity = CitizenActivity::Idle;
        SettlementCommandId assignedCommandId;
    };

    class SettlementCitizenState
    {
    public:
        [[nodiscard]]
        bool initialize(
            std::uint64_t citizenCount,
            std::uint64_t nameSeed
        );

        void placeUnpositionedCitizens(
            const SettlementMap& settlementMap
        );

        [[nodiscard]]
        CitizenId assignIdleCitizen(
            SettlementCommandId commandId
        ) noexcept;

        void releaseCommand(
            SettlementCommandId commandId
        ) noexcept;

        [[nodiscard]]
        std::span<const SettlementCitizen> citizens() const noexcept;

        [[nodiscard]]
        const SettlementCitizen* citizen(CitizenId id) const noexcept;

        [[nodiscard]]
        const SettlementCitizen* citizenAt(
            SettlementTilePosition position
        ) const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

    private:
        std::vector<SettlementCitizen> citizens_;
        IdGenerator<CitizenId> citizenIds_;
        std::uint64_t version_ = 0;
    };
}
