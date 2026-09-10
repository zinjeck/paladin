#pragma once
#include "simulation/systems/SettlementFamilySystem.h"
#include "simulation/systems/SettlementJobBoard.h"

#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/stockpile/StockpileJob.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include <vector>

namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    struct SettlementCitizen;

    enum class CitizenTaskKind
    {
        None,
        Eat,
        Haul,
        Gather,
        Demolish,
        Build,
        Work,
        Home,
        Sleep,
        Break,
        Talk,
        Care,
        FamilyMeal,
        AnimalWork
    };
    struct CitizenTask
    {
        CitizenTaskKind kind = CitizenTaskKind::None;
        InventoryId source;
        InventoryId destination;
        ConstructionSiteId site;
        SettlementObjectId object;
        SettlementCommandId command;
        SettlementTilePosition target{-1, -1};
        SettlementTilePosition workTile{-1, -1};
        double laborMinutes = 0;
        double startedMinute = 0;
        bool delivering = false;
        CitizenId partner;
        EntityId animal;
        std::string partnerName;
        double endMinute = 0;
    };
    struct CitizenSimulationPolicy
    {
        // Probability per full day of eligible time for each mother.
        double dailyBirthChance = .055;
        double childMaturationMinutes = 2 * 1440;
        double adultYearMinutes = 18 * 1440;
        std::uint16_t adulthoodAge = 16;
        std::size_t maximumDependentChildrenPerCouple = 2;
        double childcareWanderMinutes = 4;
        double toddlerCareHealthPerDay = 6;
        double toddlerCareHappinessPerDay = 12;
        double toddlerNeglectHealthPerDay = 12;
        double toddlerNeglectHappinessPerDay = 20;
        double childcareAbsenceGraceMinutes = 60;
        double childcareHealthGraceMinutes = 180;
        double childcareRecoveryMinutesPerMinute = 2;
        double caregiverMinimumHealth = 25;
        std::uint16_t fertilityEndAge = 45;
        double parentHealthThreshold = 80;
        std::uint16_t independentEatingAge = 5;
        double nursingFoodShare = .15;
        double nursingHungerPerMinute = 10;
        double dependentFoodShare = .5;
        double familyMealTimeoutMinutes = 180;
        int leisureRadius = 8;
        int childNeighborhoodRadius = 4;
        double familiarityPerTalkMinute = .4;
        double maximumFamiliarity = 100;
        double familiarityPreference = .03;
        double hungerPerDay = 100;
        double foodSeekThreshold = 50;
        double urgentFoodThreshold = 70;
        double requiredSleepMinutes = 5 * 60;
        double awakeEnergyPerMinute = 25.0 / (16 * 60);
        double workEnergyPerMinute = 25.0 / 720;
        double sleepEnergyPerMinute = 50.0 / 300;
        double fatigueHealthPerDay = 20;
        double workBreakMinutes = 30;
        double starvationThreshold = 75;
        double mealRestoration = 50;
        double healthRecoveryPerDay = 25;
        double happinessRecoveryPerDay = 12;
        double unemploymentHappinessPerDay = 2.5;
        double talkingHappinessPerMinute = .1;
        double postWorkLeisureShare = .65;
        double fullRestEnergy = 100;
        double fatigueEnergy = 50;
        double criticalRestEnergy = 25;
        double retryMinutes = 5;
        int shiftStartMinute = 6 * 60;
        int shiftEndMinute = 18 * 60;
        int localSearchRadius = 24;
        StockpileJobPolicy stockpile;
        int carryingCapacity = 4;
        double gatheringMinutes = 15;
        double demolitionMinutes = 20;
        double constructionMinutes = 45;
        double roadMinutes = 2;
        FisheryJobPolicy fishery;
        std::size_t decisionsPerMinute = 32;
        std::size_t pathsPerMinute = 24;
        // Make the existing per-minute path budget available in one simulation
        // step. This lets a group of already-idle general laborers respond to a
        // distant player designation together instead of leaking out four at a
        // time over successive minutes. Active citizen tasks are still left
        // alone by the ordinary decision rules.
        std::size_t maximumPathsPerStep = 24;

        void setWorkDayHours(int hours) noexcept
        {
            hours = std::clamp(hours, 0, 14);
            shiftStartMinute = 720 - hours * 30;
            shiftEndMinute = 720 + hours * 30;
        }

        double solarTimeOffsetMinutes = 0;
        double localMinute(double minute) const noexcept
        {
            const double time =
                std::fmod(minute + solarTimeOffsetMinutes, 1440.0);
            return time < 0 ? time + 1440.0 : time;
        }
        bool isWorkTime(double minute) const noexcept
        {
            const double time = localMinute(minute);
            return time >= shiftStartMinute && time < shiftEndMinute;
        }
    };

    // Sole activity authority. Employment and needs supply state, never routes
    // or competing task replacements. All exits release the same reservations.
    class SettlementActivitySystem
    {
    public:
        CitizenSimulationPolicy policy;
        bool caregivingAtWorkTime(
            const SettlementMap&,
            const SettlementCitizen&,
            double minute
        ) const;
        // A worker assigned to an empty pasture remains employed there, but is
        // temporarily available for the same civic/general labor as an
        // unemployed adult. The first contained animal ends that availability.
        bool pastureWorkerAvailableForGeneralLabor(
            const SettlementMap&,
            const SettlementCitizen&
        ) const noexcept;
        std::size_t housingCapacity() const noexcept
        {
            return families_.housingCapacity();
        }
        void tick(
            SettlementMap&,
            SettlementCitizenState&,
            double minute,
            double elapsed
        );
        static std::string activityLabel(const SettlementCitizen&);
        void synchronizeHomes(
            SettlementMap& map,
            SettlementCitizenState& citizens
        )
        {
            assignHomes(map, citizens);
        }

    private:
        enum class RouteFailureDomain : std::uint8_t
        {
            Inventory,
            Construction,
            Command,
            Animal,
            Workplace
        };
        struct RouteFailure
        {
            CitizenId citizen;
            RouteFailureDomain domain = RouteFailureDomain::Inventory;
            std::uint64_t target = 0;
            SettlementTilePosition origin;
            std::uint64_t topologyVersion = 0;
            double untilMinute = 0;
        };

        friend struct SettlementActivityTestFixture;
        bool choosePastureWork(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        void executePastureWork(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute,
            double elapsed
        );
        bool chooseAnimalWork(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        void executeAnimalWork(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute,
            double elapsed
        );
        void step(
            SettlementMap&,
            SettlementCitizenState&,
            double minute,
            double elapsed
        );
        void needs(
            const SettlementMap&,
            SettlementCitizen&,
            double elapsed,
            double minute
        );
        void planMeal(SettlementCitizen&, const SettlementMap&);
        bool shouldSleep(const SettlementCitizen&, double minute) const;
        void planSleep(SettlementMap&, SettlementCitizen&, double minute);
        bool enterHome(
            SettlementMap&,
            const SettlementCitizenState&,
            SettlementCitizen&
        );
        SettlementTilePosition sleepingPosition(
            const SettlementMap&,
            const SettlementCitizenState&,
            const SettlementCitizen&
        ) const;
        void planBreak(SettlementMap&, SettlementCitizen&, double minute);
        void startBreak(SettlementMap&, SettlementCitizen&, double minute);
        bool manageBreak(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool breakTripFits(
            SettlementMap&,
            SettlementCitizenState&,
            const SettlementCitizen&,
            const SettlementCitizen& planned,
            double minute,
            double stay
        );
        double routeMinutes(
            const SettlementMap&,
            const SettlementCitizenState&,
            const SettlementCitizen&
        ) const;
        bool chooseSocial(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool chooseWork(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool chooseSleep(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool manageToddler(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double
        );
        bool inChildNeighborhood(
            const SettlementMap&,
            const SettlementCitizen&,
            SettlementTilePosition
        ) const;
        bool childRouteIsLocal(
            const SettlementMap&,
            const SettlementCitizen&
        ) const;
        void produce(
            SettlementMap&,
            const SettlementCitizenState&,
            double minute,
            double elapsed
        );
        void assignHomes(SettlementMap& map, SettlementCitizenState& citizens);
        void finish(SettlementMap&, SettlementCitizen&, double minute);
        bool route(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            const SettlementObjectFootprint&,
            bool inside
        );
        bool routeFailed(
            const SettlementCitizen&,
            RouteFailureDomain,
            std::uint64_t target,
            SettlementTilePosition origin,
            const SettlementMap&,
            double minute
        );
        void rememberRouteFailure(
            const SettlementCitizen&,
            RouteFailureDomain,
            std::uint64_t target,
            SettlementTilePosition origin,
            const SettlementMap&,
            double minute
        );
        bool chooseFood(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool chooseHaul(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute,
            InventoryId destination = {}
        );
        bool chooseCommand(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        bool chooseConstruction(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        void decide(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute
        );
        void execute(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            double minute,
            double elapsed
        );
        bool beginHaul(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            InventoryId source,
            InventoryId destination,
            std::string_view resource,
            int amount,
            double minute
        );
        SettlementJobBoard jobBoard_;
        friend class SettlementFamilySystem;
        SettlementFamilySystem families_;
        std::vector<RouteFailure> routeFailures_;
        std::size_t decisionCursor_ = 0;
        std::size_t pathsRemaining_ = 0;
        bool routeBudgetLimited_ = false;
        double decisionCredit_ = 0;
        double pathCredit_ = 0;
    };
} // namespace Paladin