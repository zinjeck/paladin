#pragma once
#include <deque>

#include "core/StrongId.h"
#include "world/RealmLaws.h"
#include "simulation/systems/SettlementActivitySystem.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/SettlementTilePosition.h"
#include "world/entities/EntityState.h"
#include "world/settlements/citizens/SettlementAttributeReport.h"

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
        Fishing,
        Mining,
        UndergroundMining
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
        // A single personnel record underlies both civil life and enlistment.
        // Deployed people do not also work, eat or appear in their home city.
        CultureId primaryCultureId, secondaryCultureId;
        RealmId citizenshipRealmId;
        SettlementId birthSettlementId;
        [[nodiscard]] bool hasCulture(CultureId culture) const noexcept
        { return culture && (primaryCultureId==culture || secondaryCultureId==culture); }
        SoldierId soldierId;
        ArmyId militaryUnitId;
        bool militaryDeployed = false;
        bool deathRecorded = false;
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
        double housingHappinessPenalty = 0;
        double foodHappinessPenalty = 0;
        void enforceHappinessModifiers();
        SettlementObjectId exitingHomeId;
        WorkplaceId workplaceId;
        double nextWorkCheckMinutes = 0;
        SettlementTilePosition idleAnchor{-1, -1};
        SettlementTilePosition destination{-1, -1};
        std::vector<SettlementTilePosition> path;
        std::size_t pathIndex = 0;
        double stepProgress = 0;
        double stepDuration = 1;
        double walkDistance = 0;
        double workAnimationMinutes = 0;
        double idleWait = -1;
        std::uint64_t choiceSequence = 0;
        bool explicitMovement = false;
        // The citizen remains the boat occupant, not a duplicate visual actor.
        bool inFishingBoat = false;
        bool boatReturning = false;
        SettlementObjectId boatFishery;
        SettlementTilePosition boatLanding{-1, -1};
        std::vector<SettlementTilePosition> boatRoute;
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
        double homelessMinutes = 0;
        SettlementObjectId homeId;
        SettlementObjectId bedHomeId;
        int bedSlot = -1;
        bool doubleBed = false;
        double bedVisualOffsetX = 0;
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

    struct CitizenAncestry
    {
        CitizenId id, father, mother;
    };

    struct PopulationSample
    {
        double gameMinute = 0;
        std::size_t population = 0;
    };

    struct CitizenDeathRecord
    {
        std::uint64_t sequence = 0;
        CitizenId citizen;
        std::string name;
        double minute = 0;
        bool deployed = false;
    };

    struct CitizenRemains
    {
        std::uint64_t sequence=0;
        CitizenId citizen;
        std::string name;
        std::uint16_t age=0;
        CitizenSex sex=CitizenSex::Male;
        double diedMinute=0;
        SettlementTilePosition position{-1,-1},grave{-1,-1};
        SettlementObjectId graveyard;
        CitizenId carrier;
        bool pickedUp=false,buried=false;
    };

    class SettlementCitizenState
    {
        friend class SettlementCommerce;

    public:
        static std::string_view maleName(std::uint64_t index) noexcept;
        bool chooseFoundingRulerName(std::string_view name);
        const std::vector<CitizenAncestry>& ancestors() const noexcept
        {
            return ancestors_;
        }
        void rememberAncestry(const SettlementCitizen& person);
        void recordDeath(SettlementCitizen& person, double minute)
        {
            if (person.deathRecorded || person.health > 1e-7) { return; }
            person.deathRecorded = true;
            deaths_.push_back({++deathSequence_, person.id, person.name,
                               minute, person.militaryDeployed});
            if(!person.militaryDeployed && person.tilePosition.x>=0)
                remains_.push_back({deathSequence_,person.id,person.name,person.ageYears,person.sex,minute,person.tilePosition});
            while (deaths_.size() > 128) { deaths_.pop_front(); }
        }
        const std::deque<CitizenDeathRecord>& deaths() const noexcept
        {
            return deaths_;
        }
        const std::vector<CitizenRemains>& remains() const noexcept { return remains_; }
        double remainsMinute() const noexcept { return remainsMinute_; }
        const CitizenRemains* graveAt(SettlementTilePosition p) const noexcept
        { for(const auto& r:remains_) if(r.buried && r.grave==p) return &r; return nullptr; }
        std::uint64_t deathSequence() const noexcept { return deathSequence_; }

        [[nodiscard]]
        bool initialize(std::uint64_t citizenCount, std::uint64_t nameSeed);

        // Canonical personnel records remain here while soldiers are deployed,
        // but they are not residents, workers, or city population.
        [[nodiscard]] std::size_t residentCount() const noexcept
        {
            std::size_t count = 0;
            for (const auto& c : citizens_)
                if (!c.militaryDeployed && c.health > 0) ++count;
            return count;
        }
        void configureCommunity(SettlementId, RealmId, CultureId, const RealmLaws&, bool citizenshipResearched);
        [[nodiscard]] const RealmLaws& laws() const noexcept { return laws_; }
        [[nodiscard]] CultureId dominantCulture() const noexcept { return dominantCulture_; }
        [[nodiscard]] RealmId communityRealm() const noexcept { return communityRealm_; }
        [[nodiscard]] bool citizenshipResearched() const noexcept { return naturalization_; }
        [[nodiscard]] bool militaryEligible(const SettlementCitizen& c) const noexcept
        { return !c.child && c.health>0 && laws_.allowsMilitaryOrOffice(c.sex==CitizenSex::Female); }
        bool spawn(std::uint64_t count);
        bool spawnImmigrants(std::uint64_t count);
        EntityAttributes averageAttributes() const;
        void recordAttributes(double minute, double elapsed);
        const SettlementAttributeReport& attributeReport() const
        {
            return attributeReport_;
        }
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
        friend class CitizenshipSystem;
        friend class MilitarySystem;
        SettlementId community_;
        RealmId communityRealm_;
        CultureId dominantCulture_;
        RealmLaws laws_;
        bool naturalization_=false;
        friend class SettlementEmploymentState;
        friend class SettlementActivitySystem;
        friend class SettlementFamilySystem;
        friend struct SettlementActivityTestFixture;
        SettlementCitizen* mutableCitizen(CitizenId id) noexcept;
        bool appendCitizens(std::uint64_t count, bool child);
        void matchSingles();
        std::uint64_t familyVersion_ = 0;
        std::deque<PopulationSample> populationHistory_;
        std::deque<CitizenDeathRecord> deaths_;
        std::vector<CitizenRemains> remains_;
        double remainsMinute_=0;
        std::uint64_t deathSequence_ = 0;
        SettlementAttributeReport attributeReport_;
        SettlementNavigation navigation_;
        std::uint64_t behaviorSeed_ = 0;
        std::size_t decisionCursor_ = 0;
        // Increasing IDs, append-only creation and stable erasure keep this sorted.
        std::vector<SettlementCitizen> citizens_;
        std::vector<CitizenAncestry> ancestors_;
        IdGenerator<CitizenId> citizenIds_;
        std::uint64_t version_ = 0;
    };
} // namespace Paladin
