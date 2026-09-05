#pragma once
#include "simulation/systems/SettlementFamilySystem.h"
#include "simulation/systems/SettlementJobBoard.h"

#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/stockpile/StockpileJob.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <unordered_set>

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
        Talk
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
        std::string partnerName;
        double endMinute = 0;
    };
    struct CitizenSimulationPolicy
    {
        double dailyBirthChance = .03;
        double childMaturationMinutes = 3 * 1440;
        double adultYearMinutes = 12 * 1440;
        std::uint16_t adulthoodAge = 18;
        std::uint16_t fertilityEndAge = 45;
        double parentHealthThreshold = 80;
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
        double unemploymentHappinessPerDay = 2;
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
        std::size_t maximumPathsPerStep = 4;

        void setWorkDayHours(int hours) noexcept
        {
            hours = std::clamp(hours, 0, 14);
            shiftStartMinute = 720 - hours * 30;
            shiftEndMinute = 720 + hours * 30;
        }

        bool isWorkTime(double minute) const noexcept
        {
            const double time = std::fmod(minute, 1440.0);
            return time >= shiftStartMinute && time < shiftEndMinute;
        }
    };

    // Sole activity authority. Employment and needs supply state, never routes
    // or competing task replacements. All exits release the same reservations.
    class SettlementActivitySystem
    {
    public:
        CitizenSimulationPolicy policy;
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
        void step(
            SettlementMap&,
            SettlementCitizenState&,
            double minute,
            double elapsed
        );
        void needs(SettlementCitizen&, double elapsed, double minute);
        void planMeal(SettlementCitizen&, const SettlementMap&);
        bool shouldSleep(const SettlementCitizen&, double minute) const;
        void planSleep(SettlementMap&, SettlementCitizen&, double minute);
        bool enterHome(SettlementMap&, SettlementCitizen&);
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
        void produce(
            SettlementMap&,
            const SettlementCitizenState&,
            double minute,
            double elapsed
        );
        void assignHomes(SettlementMap& map, SettlementCitizenState& citizens)
        {
            families_.assignHomes(map, citizens, *this);
        }
        void finish(SettlementMap&, SettlementCitizen&, double minute);
        bool route(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementCitizen&,
            const SettlementObjectFootprint&,
            bool inside
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
        std::size_t decisionCursor_ = 0;
        std::size_t pathsRemaining_ = 0;
        bool routeBudgetLimited_ = false;
        double decisionCredit_ = 0;
        double pathCredit_ = 0;
    };
} // namespace Paladin
