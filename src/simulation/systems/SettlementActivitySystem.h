#pragma once

#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/stockpile/StockpileJob.h"
#include <cstddef>

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
    Home
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
};
struct CitizenSimulationPolicy
{
    double hungerPerDay = 50;
    double foodSeekThreshold = 50;
    double starvationThreshold = 75;
    double mealRestoration = 50;
    double healthRecoveryPerDay = 25;
    double happinessRecoveryPerDay = 12;
    double retryMinutes = 5;
    int shiftStartMinute = 8 * 60;
    int shiftEndMinute = 16 * 60;
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
    static const char* activityLabel(const SettlementCitizen&);

  private:
    void step(
        SettlementMap&,
        SettlementCitizenState&,
        double minute,
        double elapsed
    );
    void needs(SettlementCitizen&, double elapsed);
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
    bool claimed(const SettlementCitizenState&, const CitizenTask&) const;
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
    std::uint64_t housingTopology_ = ~std::uint64_t(0);
    std::size_t housedPopulation_ = ~std::size_t(0);
    std::size_t decisionCursor_ = 0;
    std::size_t pathsRemaining_ = 0;
    bool routeBudgetLimited_ = false;
    double decisionCredit_ = 0;
    double pathCredit_ = 0;
};
} // namespace Paladin
