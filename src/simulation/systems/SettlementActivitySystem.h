#pragma once

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
    double hungerPerDay = 100;
    double foodSeekThreshold = 50;
    double urgentFoodThreshold = 70;
    double requiredSleepMinutes = 5 * 60;
    double workBreakMinutes = 30;
    double starvationThreshold = 75;
    double mealRestoration = 50;
    double healthRecoveryPerDay = 25;
    double happinessRecoveryPerDay = 12;
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
    void tick(
        SettlementMap&,
        SettlementCitizenState&,
        double minute,
        double elapsed
    );
    static std::string activityLabel(const SettlementCitizen&);
    void synchronizeHomes(
        const SettlementMap& map,
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
    void needs(SettlementCitizen&, double elapsed);
    void planMeal(SettlementCitizen&, const SettlementMap&);
    bool shouldSleep(const SettlementCitizen&, double minute) const;
    void planSleep(SettlementMap&, SettlementCitizen&, double minute);
    bool enterHome(SettlementMap&, SettlementCitizen&);
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
    void assignHomes(const SettlementMap&, SettlementCitizenState&);
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
    bool claimed(const CitizenTask&) const;
    void claim(const CitizenTask&);
    void releaseClaim(const CitizenTask&);
    std::unordered_set<std::uint64_t> gatheringClaims_;
    std::unordered_set<SettlementObjectId, StrongIdHash> demolitionClaims_;
    std::unordered_set<ConstructionSiteId, StrongIdHash> siteDemolitionClaims_;
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
    struct CommandWork
    {
        SettlementCommandId command;
        SettlementObjectId object;
        ConstructionSiteId site;
        SettlementObjectFootprint footprint;
        std::size_t globalSlot = 0;
        std::size_t bucketSlot = 0;
        bool available = true;
        CitizenTask task() const;
    };
    bool refreshCommandBoard(const SettlementMap&);
    std::uint64_t commandBoardVersion_ = ~std::uint64_t(0);
    std::uint64_t commandSourceVersion_ = 0;
    std::size_t commandBuildCommand_ = 0;
    std::size_t commandBuildTarget_ = 0;
    std::size_t commandSweep_ = 0;
    bool commandBoardReady_ = false;
    std::deque<CommandWork> commandJobs_;
    std::vector<std::size_t> availableCommandJobs_;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> commandBuckets_;
    void refreshConstructionBoard(const SettlementMap&);
    std::uint64_t boardVersion_ = ~std::uint64_t(0);
    std::unordered_map<std::uint64_t, std::vector<ConstructionSiteId>>
        constructionBuckets_;
    std::unordered_map<ConstructionSiteId, std::size_t, StrongIdHash>
        clearingCursors_;
    std::uint64_t housingTopology_ = ~std::uint64_t(0);
    std::size_t housedPopulation_ = ~std::size_t(0);
    std::size_t decisionCursor_ = 0;
    std::size_t pathsRemaining_ = 0;
    bool routeBudgetLimited_ = false;
    double decisionCredit_ = 0;
    double pathCredit_ = 0;
};
} // namespace Paladin
