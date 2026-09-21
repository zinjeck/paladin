#include "simulation/AiRealmSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/MilitarySystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    int AiRealmSystem::garrisonTarget(
        const World& world,
        const Realm& realm,
        const Settlement& city
    )
    {
        std::size_t serving = 0;
        for (const auto& soldier : world.soldiers())
        {
            if (soldier.homeSettlementId() == city.id())
            {
                ++serving;
            }
        }
        const double people = double(city.population()) + serving;
        const double fraction =
            (.025 + .065 * realm.ruler.personality.militarism.first()) *
            (city.isFortress() ? 1.5 : 1.);
        const int demographic =
            int(std::clamp(std::floor(people * fraction), 0., 384.));
        const auto& stock = city.simulationState().stockpile();
        // Shrink an unsustainable guard instead of feeding it invented rations.
        double packs = 0;
        for (const auto& unit : world.armies())
        {
            if (!unit.soldierCount())
            {
                continue;
            }
            std::size_t fromHome = 0;
            for (const auto id : unit.soldiers())
            {
                if (const auto* soldier = world.soldier(id);
                    soldier && soldier->homeSettlementId() == city.id())
                {
                    ++fromHome;
                }
            }
            packs += double(unit.rations()) * fromHome / unit.soldierCount();
        }
        const double excess =
            std::max(0., stock.amount("food") - 2. * city.population()) +
            stock.amount("rations") + packs;
        return std::min(
            demographic,
            int(std::min(
                384.,
                std::floor(excess / MilitarySystem::RationsPerSoldier)
            ))
        );
    }
    void AiRealmSystem::tick(World& world, double minute, double elapsed)
    {
        if (!std::isfinite(minute) || !std::isfinite(elapsed) || elapsed <= 0)
        {
            return;
        }
        std::size_t decisions = 0;
        int shippingBudget = 1; // Never run one path search per city per frame.
        for (const auto& record : world.realms())
        {
            if (!record.aiControlled ||
                minute + elapsed < record.nextStrategyMinute)
            {
                continue;
            }
            if (decisions++ >= MaximumRealmDecisionsPerTick)
            {
                break;
            }
            const auto actor = record.id();
            auto* realm = world.realm(actor);
            realm->nextStrategyMinute =
                (std::floor((minute + elapsed) / 1440.) + 1) * 1440. +
                double(actor.value() % 24) * 5.;
            ++realm->strategyDecisions;
            for (const auto& city : world.settlements())
            {
                if (city.ownerRealmId() == actor &&
                    !city.simulationState().localMap())
                {
                    MilitarySystem::maintainStrategicGarrison(
                        world,
                        actor,
                        city.id(),
                        garrisonTarget(world, *realm, city)
                    );
                }
            }
            // Stop wasteful, lost or depleted standing routes without deleting
            // the cargo of a caravan already away from its source.
            for (const auto& route : world.shipments())
            {
                if (route.owner != actor || !route.aiManaged || !route.active())
                {
                    continue;
                }
                const auto* source = world.settlement(route.source);
                const auto* target = world.settlement(route.destination);
                if (!source || !target || source->ownerRealmId() != actor ||
                    target->ownerRealmId() != actor ||
                    WorldShipmentSystem::available(*source, route.resource) <
                        2. * source->population() ||
                    WorldShipmentSystem::total(*target, route.resource) >
                        10. * target->population() + 120)
                {
                    static_cast<void>(
                        WorldShipmentSystem::stop(world, actor, route.id)
                    );
                }
            }
            if (shippingBudget > 0)
            {
                SettlementId needy, donor;
                double bestNeed = 0;
                for (const auto& city : world.settlements())
                {
                    if (city.ownerRealmId() != actor)
                    {
                        continue;
                    }
                    const double target = double(city.population()) *
                                          (city.isFortress() ? 10. : 6.);
                    const double shortage =
                        target - WorldShipmentSystem::total(city, "food");
                    const bool supplied = std::any_of(
                        world.shipments().begin(),
                        world.shipments().end(),
                        [&](const auto& s)
                        {
                            return s.owner == actor &&
                                   s.destination == city.id() &&
                                   s.resource == "food" && s.active();
                        }
                    );
                    if (!supplied && shortage > bestNeed)
                    {
                        needy = city.id();
                        bestNeed = shortage;
                    }
                }
                double nearest = std::numeric_limits<double>::max();
                int amount = 0;
                const auto* target = world.settlement(needy);
                if (target)
                {
                    for (const auto& city : world.settlements())
                    {
                        if (city.ownerRealmId() != actor || city.id() == needy)
                        {
                            continue;
                        }
                        const double surplus =
                            WorldShipmentSystem::available(city, "food") -
                            double(city.population()) * 5.;
                        if (surplus < 10)
                        {
                            continue;
                        }
                        const double d = geographicDistance(
                            city.position(),
                            target->position(),
                            world.grid().width(),
                            world.grid().height()
                        );
                        if (d < nearest)
                        {
                            nearest = d;
                            donor = city.id();
                            amount = int(std::clamp(
                                std::min(bestNeed, surplus * .25),
                                1.,
                                500.
                            ));
                        }
                    }
                }
                if (donor)
                {
                    --shippingBudget;
                    static_cast<void>(WorldShipmentSystem::create(
                        world,
                        actor,
                        donor,
                        needy,
                        "food",
                        amount,
                        true,
                        nullptr,
                        true
                    ));
                }
            }
            if (minute + elapsed < realm->nextDiplomacyMinute)
            {
                continue;
            }
            realm->nextDiplomacyMinute =
                (std::floor((minute + elapsed) / 10080.) + 1) * 10080.;
            RealmId partner;
            int affinity = std::numeric_limits<int>::min();
            for (const auto& other : world.realms())
            {
                if (!other.aiControlled ||
                    !DiplomacySystem::inRange(world, actor, other.id()))
                {
                    continue;
                }
                const auto* relation =
                    world.diplomacy().between(actor, other.id());
                if (relation && relation->atWar)
                {
                    continue;
                }
                const int score =
                    DiplomacySystem::opinion(world, actor, other.id()) +
                    DiplomacySystem::opinion(world, other.id(), actor) +
                    int(GenerationNoise::mix(
                            world.generationSeed() ^
                            actor.value() * 73856093ULL ^
                            other.id().value() * 19349663ULL ^
                            std::uint64_t(minute / 10080)
                        ) %
                        81) -
                    40;
                if (score > affinity)
                {
                    affinity = score;
                    partner = other.id();
                }
            }
            if (!partner || realm->ruler.personality.isolationism.first() > .8)
            {
                continue;
            }
            // Only computer-computer agreements are automatic. Player choices
            // remain explicit panel actions, and no army resolves combat yet.
            const auto roll = GenerationNoise::mix(
                world.generationSeed() ^ actor.value() ^
                partner.value() * 9176ULL ^ std::uint64_t(minute / 10080)
            );
            const auto* pact = world.diplomacy().between(actor, partner);
            if (pact && pact->trading && roll % 100 < 12)
            {
                static_cast<void>(DiplomacySystem::apply(
                    world,
                    actor,
                    partner,
                    DiplomaticAction::RevokeTrade
                ));
            }
            else if ((!pact || !pact->trading) && roll % 100 < 65)
            {
                static_cast<void>(DiplomacySystem::apply(
                    world,
                    actor,
                    partner,
                    DiplomaticAction::Trade
                ));
            }
            if (DiplomacySystem::opinion(world, actor, partner) >= 10 &&
                DiplomacySystem::opinion(world, partner, actor) >= 10)
            {
                static_cast<void>(DiplomacySystem::apply(
                    world,
                    actor,
                    partner,
                    DiplomaticAction::Alliance
                ));
            }
            const auto gift =
                DiplomacySystem::suggestedGift(world, actor, partner);
            if (gift > 0 && affinity < 40)
            {
                static_cast<void>(DiplomacySystem::apply(
                    world,
                    actor,
                    partner,
                    DiplomaticAction::Gift,
                    gift
                ));
            }
        }
    }
} // namespace Paladin
