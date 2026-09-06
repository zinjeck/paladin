#include "simulation/SimulationReports.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>

namespace Paladin
{
    const CityReport* SimulationReports::city(SettlementId id) const
    {
        const auto it = cities_.find(id);
        return it == cities_.end() ? nullptr : &it->second;
    }
    void SimulationReports::emit(ReportEvent event)
    {
        events_.push_front(std::move(event));
        while (events_.size() > policy.maximumEvents)
        {
            events_.pop_back();
        }
    }
    void SimulationReports::update(
        const World& world,
        RealmId player,
        bool force
    )
    {
        const double minute = double(world.time().totalGameMinutes());
        if (!force && minute < nextRefresh_)
        {
            return;
        }
        nextRefresh_ = minute + policy.refreshMinutes;
        ++version_;
        for (const auto& realm : world.realms())
        {
            if (realm.name().empty())
            {
                continue;
            }
            const auto* capital = world.settlement(realm.capitalSettlementId());
            const RealmState current{
                std::string(realm.name()),
                std::string(realm.startingOriginId()),
                realm.capitalSettlementId(),
                capital ? capital->position() : WorldTilePosition{},
                realm.treasury->incomeTax.percent
            };
            auto [it, added] = knownRealms_.try_emplace(realm.id(), current);
            if (initialized_ && added)
            {
                emit(
                    {minute,
                     "realm-founded",
                     std::string(realm.name()) + " established a polity.",
                     {},
                     realm.id(),
                     false}
                );
            }
            else if (!added)
            {
                const auto& previous = it->second;
                if (previous.name != current.name)
                {
                    emit(
                        {minute,
                         "realm-renamed",
                         previous.name + " is now " + current.name + ".",
                         {},
                         realm.id(),
                         false}
                    );
                }
                if (previous.origin != current.origin)
                {
                    emit(
                        {minute,
                         "government",
                         current.name + " changed its political origin to " +
                             current.origin + ".",
                         {},
                         realm.id(),
                         false}
                    );
                }
                if (previous.capital != current.capital ||
                    previous.capitalPosition != current.capitalPosition)
                {
                    emit(
                        {minute,
                         "capital",
                         current.name + " changed or moved its capital.",
                         {},
                         realm.id(),
                         false}
                    );
                }
                if (previous.tax != current.tax)
                {
                    emit(
                        {minute,
                         "tax",
                         current.name + " set its realm tax to " +
                             std::to_string(current.tax) + "%.",
                         {},
                         realm.id(),
                         false}
                    );
                }
                it->second = current;
            }
        }
        ReportSample aggregate;
        aggregate.minute = minute;
        double healthPopulation = 0;
        if (const auto* realm = world.realm(player))
        {
            aggregate.gold = realm->treasury->balance / 100.0;
        }
        for (const auto& settlement : world.settlements())
        {
            auto [known, added] =
                knownCities_.try_emplace(settlement.id(), settlement.name());
            if (initialized_ && added)
            {
                emit(
                    {minute,
                     "city-founded",
                     std::string(settlement.name()) + " was founded.",
                     settlement.id(),
                     settlement.ownerRealmId(),
                     false}
                );
            }
            auto& report = cities_[settlement.id()];
            auto& sample = report.current;
            sample = {};
            sample.minute = minute;
            const auto& state = settlement.simulationState();
            sample.population = double(state.population().residents());
            const auto* map = state.localMap();
            report.detailed = map != nullptr;
            report.starving = 0;
            if (map)
            {
                const auto people = state.citizens().citizens();
                sample.population = double(people.size());
                for (const auto& c : people)
                {
                    sample.health += c.health;
                    sample.happiness += c.happiness;
                    if (c.hunger >= map->activities.policy.starvationThreshold)
                    {
                        ++report.starving;
                    }
                }
                if (!people.empty())
                {
                    sample.health /= people.size();
                    sample.happiness /= people.size();
                }
                for (const auto& inventory : map->logistics.inventories())
                {
                    if (countsAsCityStorage(inventory.kind))
                    {
                        for (const auto& goods : inventory.goods)
                        {
                            if (const auto* d =
                                    SettlementResourceCatalog::definition(
                                        goods.resource
                                    );
                                d && d->edible)
                            {
                                sample.food += goods.amount;
                            }
                        }
                    }
                }
                sample.gold = map->commerce.treasury->balance / 100.0;
                const auto warn =
                    [&](std::string key, bool active, std::string message)
                {
                    bool& previous = report.warnings[key];
                    if (active != previous)
                    {
                        emit(
                            {minute,
                             key,
                             std::string(settlement.name()) + ": " + message +
                                 (active ? "" : " (resolved)"),
                             settlement.id(),
                             settlement.ownerRealmId(),
                             true,
                             !active}
                        );
                        previous = active;
                    }
                };
                const bool founded =
                    map->logistics.founded() && !people.empty();
                warn(
                    "starvation",
                    founded && report.starving > 0,
                    "Citizens are starving"
                );
                warn(
                    "health",
                    founded && sample.health < policy.lowHealth +
                                                   (report.warnings["health"]
                                                        ? policy.recoveryMargin
                                                        : 0),
                    "Average health is low"
                );
                warn(
                    "happiness",
                    founded &&
                        sample.happiness <
                            policy.lowHappiness + (report.warnings["happiness"]
                                                       ? policy.recoveryMargin
                                                       : 0),
                    "City happiness is low"
                );
                warn(
                    "food",
                    founded && sample.food <= 0,
                    "Public food stores are empty"
                );
                warn(
                    "treasury",
                    founded && map->commerce.treasury->moneyEconomyStarted &&
                        map->commerce.treasury->balance <= 0,
                    "Treasury is empty"
                );
            }
            if (report.history.empty() ||
                minute - report.history.back().minute >= policy.sampleMinutes)
            {
                report.history.push_back(sample);
                while (report.history.size() > policy.maximumSamples)
                {
                    report.history.pop_front();
                }
            }
            if (settlement.ownerRealmId() == player)
            {
                aggregate.population += sample.population;
                aggregate.food += sample.food;
                if (map)
                {
                    aggregate.health += sample.health * sample.population;
                    aggregate.happiness += sample.happiness * sample.population;
                    healthPopulation += sample.population;
                }
            }
        }
        if (healthPopulation > 0)
        {
            aggregate.health /= healthPopulation;
            aggregate.happiness /= healthPopulation;
        }
        if (realmHistory_.empty() ||
            minute - realmHistory_.back().minute >= policy.sampleMinutes)
        {
            realmHistory_.push_back(aggregate);
            while (realmHistory_.size() > policy.maximumSamples)
            {
                realmHistory_.pop_front();
            }
        }
        initialized_ = true;
    }
} // namespace Paladin
