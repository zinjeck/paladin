#include "simulation/AiRealmSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/MilitarySystem.h"
#include "simulation/WorldMarketSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/StrategicFood.h"
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
            militaryFood(stock, double(city.population())) + packs;
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
        int patrolBudget = 1;
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
                    int fieldSoldiers = 0;
                    for (const auto& unit : world.armies())
                    {
                        if (unit.ownerRealmId() != actor || unit.garrisoned())
                        {
                            continue;
                        }
                        for (const auto id : unit.soldiers())
                        {
                            const auto* soldier = world.soldier(id);
                            fieldSoldiers +=
                                soldier &&
                                soldier->homeSettlementId() == city.id();
                        }
                    }
                    MilitarySystem::maintainStrategicGarrison(
                        world,
                        actor,
                        city.id(),
                        std::max(
                            0,
                            garrisonTarget(world, *realm, city) - fieldSoldiers
                        )
                    );
                }
            }
            // One small supplied field patrol per realm. All personnel come
            // from the real garrison; routes use ordinary land navigation.
            if (patrolBudget > 0)
            {
                ArmyId patrol, guard;
                for (const auto& unit : world.armies())
                {
                    if (unit.ownerRealmId() != actor || !unit.soldierCount())
                    {
                        continue;
                    }
                    if (!unit.garrisoned())
                    {
                        patrol = unit.id();
                        break;
                    }
                    if (!guard && unit.soldierCount() >= 8)
                    {
                        guard = unit.id();
                    }
                }
                if (!patrol && guard)
                {
                    patrol = MilitarySystem::formStrategicPatrol(
                        world,
                        actor,
                        guard
                    );
                }
                const auto* unit = world.army(patrol);
                if (unit && !unit->moving() && !unit->engagedOpponent())
                {
                    // Claim the shared budget before trying a destination. An
                    // isolated friendly city must not leave this patrol idle
                    // forever or trigger one failed search per realm/frame.
                    --patrolBudget;
                    const auto origin = unit->position();
                    WorldTilePosition destination = origin;
                    double nearest = 33;
                    for (const auto& city : world.settlements())
                    {
                        const double distance = worldLandStepDistance(
                            origin, city.position(), world.grid().width());
                        if (city.ownerRealmId() == actor && distance > 1 &&
                            distance < nearest)
                        {
                            nearest = distance;
                            destination = city.position();
                        }
                    }
                    const bool ordered = destination != origin &&
                        MilitarySystem::orderMove(world, actor, patrol, destination) ==
                            MilitaryResult::Success;
                    if (!ordered)
                    {
                        const auto choice = GenerationNoise::mix(
                            actor.value() ^ std::uint64_t(minute / 1440));
                        constexpr WorldTilePosition
                            directions[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                        const int width = world.grid().width();
                        for (int i = 0; i < 4; ++i)
                        {
                            const auto d = directions[(choice + i) % 4];
                            auto candidate = origin;
                            // Check every leg, not just a dry endpoint beyond
                            // water/mountains. Longitude wraps; latitude does not.
                            for (int step = 0; step < 3; ++step)
                            {
                                const WorldTilePosition next{
                                    (candidate.x + d.x + width) % width,
                                    candidate.y + d.y};
                                if (!worldLandStepAllowed(world.grid(), candidate, next))
                                    break;
                                candidate = next;
                            }
                            if (candidate == origin) continue;
                            // At most one short, known-reachable fallback after
                            // the one city route search, never a world-wide scan.
                            static_cast<void>(MilitarySystem::orderMove(
                                world, actor, patrol, candidate));
                            break;
                        }
                    }
                }
            }
            // Stop wasteful, lost or depleted standing routes without deleting
            // the cargo of a caravan already away from its source.
            for (const auto& route : world.shipments())
            {
                if (route.owner != actor || !route.aiManaged || route.buyer ||
                    !route.active())
                {
                    continue;
                }
                const auto* source = world.settlement(route.source);
                const auto* target = world.settlement(route.destination);
                if (!source || !target || source->ownerRealmId() != actor ||
                    target->ownerRealmId() != actor ||
                    WorldShipmentSystem::available(*source, route.resource) <=
                        0 ||
                    civilianFood(source->simulationState().stockpile()) <
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
                std::string resource;
                int amount = 0;
                double priority = 0;
                for (const auto& target : world.settlements())
                {
                    if (target.ownerRealmId() != actor)
                    {
                        continue;
                    }
                    for (const auto& food :
                         SettlementResourceCatalog::definitions())
                    {
                        if (!food.edible || food.emergencyOnly)
                        {
                            continue;
                        }
                        const auto demand =
                            WorldMarketSystem::quote(world, target, food.id);
                        if (demand.wanted < 1)
                        {
                            continue;
                        }
                        const bool supplied = std::any_of(
                            world.shipments().begin(),
                            world.shipments().end(),
                            [&](const auto& route)
                            {
                                return route.owner == actor &&
                                       route.destination == target.id() &&
                                       route.resource == food.id &&
                                       route.active();
                            }
                        );
                        if (supplied)
                        {
                            continue;
                        }
                        for (const auto& source : world.settlements())
                        {
                            if (source.ownerRealmId() != actor ||
                                source.id() == target.id())
                            {
                                continue;
                            }
                            const double surplus = std::min(
                                double(WorldShipmentSystem::available(
                                    source,
                                    food.id
                                )),
                                WorldMarketSystem::quote(world, source, food.id)
                                    .offered
                            );
                            const double distance = geographicDistance(
                                source.position(),
                                target.position(),
                                world.grid().width(),
                                world.grid().height()
                            );
                            const int load =
                                int(std::min({demand.wanted, surplus, 500.}));
                            const double score = load / (1 + distance);
                            if (load > 0 && score > priority)
                            {
                                needy = target.id();
                                donor = source.id();
                                resource = food.id;
                                amount = load;
                                priority = score;
                            }
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
                        resource,
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
