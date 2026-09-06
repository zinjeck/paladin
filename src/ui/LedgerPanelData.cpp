#include "simulation/Simulation.h"
#include "ui/LedgerPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace Paladin
{
    namespace
    {
        LedgerCell text(std::string_view value)
        {
            return {std::string(value)};
        }
        LedgerCell number(double value, int decimals = 0)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(decimals) << value;
            return {out.str(), value, true};
        }
    } // namespace
    void LedgerPanel::buildRows(const Simulation& sim)
    {
        rows_.clear();
        headings_.clear();
        samples_.clear();
        const auto& world = sim.world();
        const auto* settlement = world.settlement(city_);
        const auto* map = sim.settlementMap(city_);
        const auto* report = sim.reports.city(city_);
        title_ = world_ ? "World Ledger" : "City Ledger";
        if (events_)
        {
            title_ = world_ ? "World Events" : "City Events";
            int active = 0;
            for (const auto& city : world.settlements())
            {
                if (world_ ? city.ownerRealmId() == sim.playerRealmId()
                           : city.id() == city_)
                {
                    if (const auto* r = sim.reports.city(city.id()))
                    {
                        for (const auto& [key, on] : r->warnings)
                        {
                            if (on)
                            {
                                ++active;
                            }
                        }
                    }
                }
            }
            subtitle_ = std::to_string(active) +
                        " active warnings | Recent events, "
                        "newest first | Scroll for history";
            for (const auto& event : sim.reports.events())
            {
                if (!world_ && event.city != city_)
                {
                    continue;
                }
                const std::string stamp =
                    "Day " + std::to_string(int(event.minute / 1440) + 1) +
                    " " + std::to_string(int(event.minute) % 1440 / 60) + ":" +
                    (int(event.minute) % 60 < 10 ? "0" : "") +
                    std::to_string(int(event.minute) % 60) + " - " +
                    (event.resolved  ? "Resolved"
                     : event.warning ? "Warning"
                                     : "News");
                rows_.push_back(
                    {std::uint64_t(rows_.size()),
                     {text(stamp), text(event.text)}}
                );
            }
            return;
        }
        if (page_ == 2)
        {
            samples_ = world_   ? sim.reports.realmHistory()
                       : report ? report->history
                                : std::deque<ReportSample>{};
            subtitle_ =
                world_
                    ? "Your realm: population and treasury; health/food from "
                      "mapped settlements"
                    : "Hourly snapshots | Treasury is shared with your realm";
            return;
        }
        subtitle_ = "Click a column to sort; click again to reverse. Mouse "
                    "wheel scrolls.";
        if (!world_)
        {
            if (!settlement || !map)
            {
                subtitle_ = "No active settlement";
                return;
            }
            const auto& people = settlement->simulationState().citizens();
            if (page_ == 0)
            {
                headings_ = {
                    "Name",
                    "Age",
                    "Health",
                    "Hunger",
                    "Energy",
                    "Happy",
                    "Activity",
                    "Gold"
                };
                for (const auto& c : people.citizens())
                {
                    rows_.push_back(
                        {c.id.value(),
                         {text(c.name),
                          number(c.ageYears),
                          number(c.health),
                          number(c.hunger),
                          number(c.energy),
                          number(c.happiness),
                          text(SettlementActivitySystem::activityLabel(c)),
                          c.child
                              ? text("Dependent")
                              : number(
                                    map->commerce.spendingBalance(c, people) /
                                        100.0,
                                    2
                                )}}
                    );
                }
                subtitle_ = "All citizens | Hunger: lower is better | Married "
                            "adults show shared spendable gold";
            }
            else if (page_ == 1)
            {
                headings_ =
                    {"Resource", "In storage", "Produced", "Meals used"};
                std::unordered_map<std::string, double> stored;
                for (const auto& inventory : map->logistics.inventories())
                {
                    if (countsAsCityStorage(inventory.kind))
                    {
                        for (const auto& goods : inventory.goods)
                        {
                            stored[goods.resource] += goods.amount;
                        }
                    }
                }
                for (const auto& resource :
                     SettlementResourceCatalog::definitions())
                {
                    const auto it = map->commerce.resourceTotals().find(
                        std::string(resource.id)
                    );
                    const auto totals =
                        it == map->commerce.resourceTotals().end()
                            ? SettlementCommerce::ResourceTotals{}
                            : it->second;
                    rows_.push_back(
                        {std::uint64_t(rows_.size()),
                         {text(resource.displayName),
                          number(stored[std::string(resource.id)]),
                          number(totals.produced),
                          number(totals.consumed)}}
                    );
                }
                subtitle_ = "Recorded totals since founding; transfers are not "
                            "production. Storage: keep/stockpiles only.";
            }
            else
            {
                headings_ = {
                    "Workplace",
                    "State",
                    "Workers",
                    "Slots",
                    "Max slots",
                    "Area"
                };
                for (const auto& job : map->employment().workplaces())
                {
                    rows_.push_back(
                        {job.id.value(),
                         {text(job.name.empty() ? job.objectTypeId : job.name),
                          text(
                              job.operational ? "Operational" : "Construction"
                          ),
                          number(
                              static_cast<double>(
                                  map->employment().employed(job.id, people)
                              )
                          ),
                          number(job.capacity),
                          number(job.maximumCapacity),
                          number(
                              double(job.footprint.width) * job.footprint.height
                          )}}
                    );
                }
            }
            return;
        }
        struct RealmTotals
        {
            double cities = 0, population = 0;
        };
        std::unordered_map<RealmId, RealmTotals, StrongIdHash> totals;
        for (const auto& city : world.settlements())
        {
            auto& total = totals[city.ownerRealmId()];
            ++total.cities;
            const auto* r = sim.reports.city(city.id());
            total.population +=
                r ? r->current.population
                  : double(city.simulationState().population().residents());
        }
        if (page_ == 0)
        {
            headings_ = {
                "Realm",
                "Origin",
                "Culture",
                "Cities",
                "Population",
                "Gold",
                "Capital"
            };
            for (const auto& realm : world.realms())
            {
                if (realm.name().empty())
                {
                    continue;
                }
                const auto* culture = world.culture(realm.primaryCultureId());
                const auto* capital =
                    world.settlement(realm.capitalSettlementId());
                rows_.push_back(
                    {realm.id().value(),
                     {text(realm.name()),
                      text(realm.startingOriginId()),
                      text(culture ? culture->name() : "None"),
                      number(totals[realm.id()].cities),
                      number(totals[realm.id()].population),
                      number(realm.treasury->balance / 100.0, 2),
                      text(capital ? capital->name() : "None")}}
                );
            }
            subtitle_ = "All existing realms, including yours. No placeholder "
                        "or invented AI polities.";
        }
        else if (page_ == 1)
        {
            headings_ = {
                "Settlement",
                "Realm",
                "Population",
                "Health",
                "Happiness",
                "Food",
                "Simulation"
            };
            for (const auto& city : world.settlements())
            {
                const auto* realm = world.realm(city.ownerRealmId());
                const auto* r = sim.reports.city(city.id());
                rows_.push_back(
                    {city.id().value(),
                     {text(city.name()),
                      text(realm ? realm->name() : "None"),
                      number(
                          r ? r->current.population
                            : double(city.simulationState()
                                         .population()
                                         .residents())
                      ),
                      r && r->detailed ? number(r->current.health)
                                       : text("N/A"),
                      r && r->detailed ? number(r->current.happiness)
                                       : text("N/A"),
                      r && r->detailed ? number(r->current.food) : text("N/A"),
                      text(
                          city.id() == sim.detailedSimulationSettlementId()
                              ? "Detailed"
                          : r && r->detailed ? "Inactive"
                                             : "Aggregate"
                      )}}
                );
            }
            subtitle_ = "Health/food use mapped-city snapshots; aggregate-only "
                        "data is marked N/A.";
        }
        else
        {
            headings_ =
                {"Realm", "Origin", "Capital", "Realm tax %", "Work hours"};
            for (const auto& realm : world.realms())
            {
                if (realm.name().empty())
                {
                    continue;
                }
                const auto* capital =
                    world.settlement(realm.capitalSettlementId());
                rows_.push_back(
                    {realm.id().value(),
                     {text(realm.name()),
                      text(realm.startingOriginId()),
                      text(capital ? capital->name() : "None"),
                      number(realm.treasury->incomeTax.percent),
                      number(realm.workDayHours())}}
                );
            }
            subtitle_ = "Existing political and fiscal state. Diplomacy, wars "
                        "and relations are not implemented yet.";
        }
    }
} // namespace Paladin
