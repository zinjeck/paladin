#pragma once
#include <deque>

#include "core/StrongId.h"
#include "simulation/systems/SettlementActivitySystem.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/SettlementTilePosition.h"
#include "world/entities/EntityState.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
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
        AtHome,
        Sleeping,
        OnBreak,
        Talking,
        Fishing
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
    struct SettlementCitizen : EntityState
    {
        // Sparse relationships: only citizens who have actually conversed.
        std::unordered_map<CitizenId, double, StrongIdHash> familiarities;
        double familiarityWith(CitizenId other) const
        {
            const auto it = familiarities.find(other);
            return it == familiarities.end() ? 0 : it->second;
        }
        CitizenSex sex = CitizenSex::Male;
        CitizenActivity activity = CitizenActivity::Idle;
        SettlementCommandId assignedCommandId;
        std::uint16_t ageYears = 25;
        bool child = false;
        double ageMinutes = 0;
        CitizenId spouseId;
        CitizenId motherId;
        CitizenId fatherId;
        // Kinship is separate from dependency; avoid matching close relatives.
        CitizenId birthMotherId;
        CitizenId birthFatherId;
        double fertilityExposure = 0;
        double fertilityTarget = 0;
        std::uint64_t birthSequence = 0;
        CitizenId caregiverId;
        // Scheduling history, not a need/stat: brief essential trips are safe.
        double unsupervisedMinutes = 0;
        int youngDependents = 0;
        double publicMealShare = 0;
        double publicFoodDissatisfaction = 0;
        double taxHappinessAdjustment = 0;
        SettlementObjectId exitingHomeId;
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
        double restThreshold = 0;
        double foodSeekHunger = -1;
        std::uint64_t mealSequence = 0;
        double sleptMinutes = 0;
        double nextHomeWander = 0;
        std::int64_t breakDay = -1;
        double breakDue = 0;
        double breakUntil = 0;
        bool breakTaken = false;
        bool breakReturning = false;
        WorkplaceId breakEmployer;
        SettlementObjectId breakObject;
        SettlementTilePosition breakAnchor{-1, -1};
        double nextBreakWander = 0;
        double nextSocialMinute = 0;
        bool insideHome = false;
        SettlementTilePosition homeEntrance{-1, -1};
        double happiness = 100;
        double homelessMinutes = 0;
        SettlementObjectId homeId;
        CitizenTask task;
        std::string carriedResource;
        std::vector<SettlementTilePosition> haulDeliveryPath;
        SettlementTilePosition haulDeliveryTarget;
        std::uint64_t haulDeliveryTopology = 0;
        int carriedAmount = 0;
        double nextDecisionMinute = 0;
        std::uint64_t observedLogisticsVersion = 0;

        double visualX() const noexcept;
        double visualY() const noexcept;
    };

    struct PopulationSample
    {
        double gameMinute = 0;
        std::size_t population = 0;
    };

    class SettlementCitizenState
    {
        friend class SettlementCommerce;

    public:
        [[nodiscard]]
        bool initialize(std::uint64_t citizenCount, std::uint64_t nameSeed);

        bool spawn(std::uint64_t count);
        void recordPopulation(double minute);
        const std::deque<PopulationSample>& populationHistory() const noexcept
        {
            return populationHistory_;
        }
        const SettlementNavigation& navigationDiagnostics() const noexcept
        {
            return navigation_;
        }

        void placeUnpositionedCitizens(const SettlementMap& settlementMap);

        [[nodiscard]]
        CitizenId assignIdleCitizen(SettlementCommandId commandId) noexcept;

        void releaseCommand(SettlementCommandId commandId) noexcept;

        [[nodiscard]]
        std::span<const SettlementCitizen> citizens() const noexcept;
        void captureVisualPositions()
        {
            for (auto& c : citizens_)
            {
                c.captureVisual(c.visualX(), c.visualY());
            }
        }

        [[nodiscard]]
        const SettlementCitizen* citizen(CitizenId id) const noexcept;

        [[nodiscard]]
        const SettlementCitizen* citizenAt(
            SettlementTilePosition position
        ) const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

        void resetLocalPlacement() noexcept;
        void tickMovement(
            const SettlementMap& map,
            double gameMinutes,
            const std::function<
                void(const SettlementCitizen&, SettlementTilePosition)>&
                onStep = {}
        );
        bool moveTo(
            CitizenId id,
            const SettlementMap& map,
            SettlementTilePosition destination
        );
        CitizenMovementPolicy movementPolicy;
        CitizenIdlePolicy idlePolicy;

    private:
        friend class SettlementEmploymentState;
        friend class SettlementActivitySystem;
        friend class SettlementFamilySystem;
        friend struct SettlementActivityTestFixture;
        bool appendCitizens(std::uint64_t count, bool child);
        void matchSingles();
        std::uint64_t familyVersion_ = 0;
        std::deque<PopulationSample> populationHistory_;
        SettlementNavigation navigation_;
        std::uint64_t behaviorSeed_ = 0;
        std::size_t decisionCursor_ = 0;
        std::vector<SettlementCitizen> citizens_;
        IdGenerator<CitizenId> citizenIds_;
        std::uint64_t version_ = 0;
    };
} // namespace Paladin
