#pragma once

#include "core/StrongId.h"
#include "simulation/systems/SettlementNavigation.h"
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

    struct CitizenIdlePolicy
    {
        double minimumWaitMinutes = 5;
        double maximumWaitMinutes = 15;
        double standProbability = .35;
        int anchorRadius = 4;
        int destinationRadius = 4;
        std::size_t maximumPathSteps = 6;
        std::size_t maximumExpandedNodes = 96;
        std::size_t decisionsPerTick = 64;
    };

    struct SettlementCitizen
    {
        CitizenId id;
        std::string name;
        CitizenSex sex = CitizenSex::Male;
        SettlementTilePosition tilePosition{-1, -1};
        CitizenActivity activity = CitizenActivity::Idle;
        SettlementCommandId assignedCommandId;
        SettlementTilePosition idleAnchor{-1, -1};
        SettlementTilePosition destination{-1, -1};
        std::vector<SettlementTilePosition> path;
        std::size_t pathIndex = 0;
        double stepProgress = 0;
        double stepDuration = 1;
        double idleWait = -1;
        std::uint64_t choiceSequence = 0;
        bool explicitMovement = false;
        double visualX() const noexcept;
        double visualY() const noexcept;
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

        void tickMovement(const SettlementMap& map, double gameMinutes);
        bool moveTo(CitizenId id, const SettlementMap& map, SettlementTilePosition destination);
        CitizenMovementPolicy movementPolicy;
        CitizenIdlePolicy idlePolicy;

    private:
        SettlementNavigation navigation_;
        std::uint64_t behaviorSeed_ = 0;
        std::size_t decisionCursor_ = 0;
        std::vector<SettlementCitizen> citizens_;
        IdGenerator<CitizenId> citizenIds_;
        std::uint64_t version_ = 0;
    };
}
