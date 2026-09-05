#pragma once

#include "core/StrongId.h"
#include "simulation/systems/SettlementActivitySystem.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/SettlementTilePosition.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Paladin
{
    class SettlementMap;
    class SettlementEmploymentState;

    enum class CitizenSex : std::uint8_t
    {
        Male,
        Female
    };

    enum class CitizenActivity : std::uint8_t
    {
        Idle,
        AssignedToCommand,
        TravelingToWork,
        AtWork,
        SeekingFood,
        Hauling,
        Constructing,
        ReturningHome,
        AtHome
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

    struct CitizenRouteFailure
    {
        InventoryId source;
        SettlementTilePosition origin;
        std::uint64_t topologyVersion = 0;
        double untilMinute = 0;
    };
    struct SettlementCitizen
    {
        CitizenId id;
        std::string name;
        CitizenSex sex = CitizenSex::Male;
        SettlementTilePosition tilePosition{-1, -1};
        CitizenActivity activity = CitizenActivity::Idle;
        SettlementCommandId assignedCommandId;
        std::uint16_t ageYears = 20; // No aging until the lifecycle system is enabled.
        WorkplaceId workplaceId;
        double nextWorkCheckMinutes = 0;
        SettlementTilePosition idleAnchor{-1, -1};
        SettlementTilePosition destination{-1, -1};
        std::vector<SettlementTilePosition> path;
        std::size_t pathIndex = 0;
        double stepProgress = 0;
        double stepDuration = 1;
        double idleWait = -1;
        std::uint64_t choiceSequence = 0;
        bool explicitMovement = false;
        std::size_t constructionSearchCursor = 0;
        std::size_t commandSearchCursor = 0;
        std::vector<CitizenRouteFailure> routeFailures;
        double health = 100;
        double hunger = 0;
        double happiness = 100;
        double homelessMinutes = 0;
        SettlementObjectId homeId;
        CitizenTask task;
        std::string carriedResource;
        int carriedAmount = 0;
        double nextDecisionMinute = 0;
        std::uint64_t observedLogisticsVersion = 0;

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

        bool spawn(std::uint64_t count);
        const SettlementNavigation& navigationDiagnostics() const noexcept
        {
            return navigation_;
        }

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

        void resetLocalPlacement() noexcept;
        void tickMovement(const SettlementMap& map, double gameMinutes);
        bool moveTo(CitizenId id, const SettlementMap& map, SettlementTilePosition destination);
        CitizenMovementPolicy movementPolicy;
        CitizenIdlePolicy idlePolicy;

    private:
        friend class SettlementEmploymentState;
        friend class SettlementActivitySystem;
        friend struct SettlementActivityTestFixture;
        SettlementNavigation navigation_;
        std::uint64_t behaviorSeed_ = 0;
        std::size_t decisionCursor_ = 0;
        std::vector<SettlementCitizen> citizens_;
        IdGenerator<CitizenId> citizenIds_;
        std::uint64_t version_ = 0;
    };
}
