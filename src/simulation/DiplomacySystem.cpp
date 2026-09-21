#include "simulation/DiplomacySystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace Paladin
{
    namespace
    {
        std::uint64_t population(const World& world, RealmId realm)
        {
            std::uint64_t result = 0;
            for (const auto& city : world.settlements())
            {
                if (city.ownerRealmId() == realm)
                {
                    result += city.population();
                }
            }
            return result;
        }
        int initialOpinion(const World& world, RealmId observer, RealmId target)
        {
            const auto* a = world.realm(observer);
            const auto* b = world.realm(target);
            if (!a || !b)
            {
                return 0;
            }
            const auto& x = a->ruler.personality;
            const auto& y = b->ruler.personality;
            const double similarity =
                1 - (std::abs(x.militarism.first() - y.militarism.first()) +
                     std::abs(
                         x.authoritarianism.first() - y.authoritarianism.first()
                     ) +
                     std::abs(x.elitism.first() - y.elitism.first())) /
                        3;
            const int affinity =
                a->primaryCultureId() &&
                        a->primaryCultureId() == b->primaryCultureId()
                    ? 15
                    : 0;
            return std::clamp(
                int(std::lround(
                    24 * similarity - 20 * x.isolationism.first() - 7
                )) + affinity,
                -100,
                100
            );
        }
    } // namespace
    double DiplomacySystem::distance(const World& world, RealmId a, RealmId b)
    {
        double result = 3.141592653589793;
        if (!world.realm(a) || !world.realm(b))
        {
            return result;
        }
        for (const auto& x : world.settlements())
        {
            if (x.ownerRealmId() == a)
            {
                for (const auto& y : world.settlements())
                {
                    if (y.ownerRealmId() == b)
                    {
                        result = std::min(
                            result,
                            geographicDistance(
                                x.position(),
                                y.position(),
                                world.grid().width(),
                                world.grid().height()
                            )
                        );
                    }
                }
            }
        }
        return result;
    }
    bool DiplomacySystem::inRange(const World& world, RealmId a, RealmId b)
    {
        return a && b && a != b && distance(world, a, b) <= RangeRadians;
    }
    int DiplomacySystem::opinion(
        const World& world,
        RealmId observer,
        RealmId target
    )
    {
        if (observer == target && observer)
        {
            return 100;
        }
        if (const auto* r = world.diplomacy().between(observer, target))
        {
            return observer == r->first ? r->firstOpinion : r->secondOpinion;
        }
        return initialOpinion(world, observer, target);
    }
    Money DiplomacySystem::suggestedGift(
        const World& world,
        RealmId actor,
        RealmId target
    )
    {
        const auto* a = world.realm(actor);
        const auto* b = world.realm(target);
        if (!a || !b || !a->treasury || actor == target ||
            a->treasury->balance <= 0)
        {
            return 0;
        }
        // A modest gift scales with BOTH populations and never exceeds 5% of
        // the sender's real treasury. Money is not created by this estimate.
        const long double size = std::sqrt(
            static_cast<long double>(population(world, actor)) *
            population(world, target)
        );
        const auto ideal = Money(std::min(100000000.L, 50.L + 50.L * size));
        return std::min(ideal, a->treasury->balance / 20);
    }
    DiplomaticResult DiplomacySystem::apply(
        World& world,
        RealmId actor,
        RealmId target,
        DiplomaticAction action,
        Money gift
    )
    {
        auto* a = world.realm(actor);
        auto* b = world.realm(target);
        if (!a || !b)
        {
            return DiplomaticResult::InvalidRealm;
        }
        if (actor == target)
        {
            return DiplomaticResult::Self;
        }
        if (!inRange(world, actor, target))
        {
            return DiplomaticResult::OutOfRange;
        }
        auto& state = world.diplomacy();
        auto relation = state.between(actor, target)
                            ? *state.between(actor, target)
                            : DiplomaticRelation{actor, target};
        if (!state.between(actor, target))
        {
            relation.firstOpinion = initialOpinion(world, actor, target);
            relation.secondOpinion = initialOpinion(world, target, actor);
        }
        const auto changeOpinion = [&](RealmId observer, int amount)
        {
            auto& value = observer == relation.first ? relation.firstOpinion
                                                     : relation.secondOpinion;
            value = std::clamp(value + amount, -100, 100);
        };
        if (action == DiplomaticAction::Gift)
        {
            if (gift <= 0)
            {
                return DiplomaticResult::InvalidGift;
            }
            if (!a->treasury || !b->treasury || a->treasury->balance < gift)
            {
                return DiplomaticResult::InsufficientGold;
            }
            if (b->treasury->balance > std::numeric_limits<Money>::max() - gift)
            {
                return DiplomaticResult::TreasuryOverflow;
            }
            // Preflight both sides before mutation; no gold is created or
            // destroyed.
            const Money expected =
                std::max<Money>(1, suggestedGift(world, actor, target));
            const int gratitude = int(std::clamp(
                10.L * static_cast<long double>(gift) / expected,
                1.L,
                25.L
            ));
            a->treasury->moneyEconomyStarted = true;
            a->treasury->balance -= gift;
            b->treasury->balance += gift;
            b->treasury->moneyEconomyStarted = true;
            changeOpinion(target, gratitude);
        }
        switch (action)
        {
        case DiplomaticAction::Alliance:
            if (relation.atWar)
            {
                return DiplomaticResult::AtWar;
            }
            if (relation.allied)
            {
                return DiplomaticResult::Already;
            }
            relation.allied = true;
            changeOpinion(actor, 15);
            changeOpinion(target, 15);
            break;
        case DiplomaticAction::Trade:
            if (relation.atWar)
            {
                return DiplomaticResult::AtWar;
            }
            if (relation.trading)
            {
                return DiplomaticResult::Already;
            }
            relation.trading = true;
            changeOpinion(actor, 10);
            changeOpinion(target, 10);
            break;
        case DiplomaticAction::RevokeTrade:
            if (!relation.trading)
            {
                return DiplomaticResult::Already;
            }
            relation.trading = false;
            changeOpinion(target, -5);
            break;
        case DiplomaticAction::Tribute:
            if (relation.overlord == actor && relation.tributary == target)
            {
                relation.overlord = {};
                relation.tributary = {};
                changeOpinion(target, 20);
                break;
            }
            if (relation.atWar)
            {
                return DiplomaticResult::AtWar;
            }
            if (state.overlordOf(target))
            {
                return DiplomaticResult::SubjectConflict;
            }
            // Reject self-cycles and indirect cycles in the subject graph.
            {
                auto parent = actor;
                std::size_t remaining = world.realms().size() + 1;
                while (parent)
                {
                    if (parent == target || remaining-- == 0)
                    {
                        return DiplomaticResult::SubjectConflict;
                    }
                    parent = state.overlordOf(parent);
                }
            }
            relation.overlord = actor;
            relation.tributary = target;
            changeOpinion(target, -30);
            break;
        case DiplomaticAction::War:
            if (relation.atWar)
            {
                return DiplomaticResult::Already;
            }
            relation.atWar = true;
            relation.allied = relation.trading = false;
            relation.overlord = relation.tributary = {};
            relation.firstOpinion = relation.secondOpinion = -100;
            break;
        case DiplomaticAction::Peace:
            if (!relation.atWar)
            {
                return DiplomaticResult::Already;
            }
            relation.atWar = false;
            changeOpinion(actor, 25);
            changeOpinion(target, 25);
            break;
        case DiplomaticAction::Gift:
            break;
        }
        for (auto& r : state.relations)
        {
            if ((r.first == actor && r.second == target) ||
                (r.first == target && r.second == actor))
            {
                r = relation;
                return DiplomaticResult::Success;
            }
        }
        state.relations.push_back(relation);
        return DiplomaticResult::Success;
    }
    bool DiplomacySystem::hostileContact(
        World& world,
        RealmId actor,
        RealmId target
    )
    {
        if (!actor || !target || actor == target || !world.realm(actor) ||
            !world.realm(target))
        {
            return false;
        }
        auto& relations = world.diplomacy().relations;
        auto found = std::find_if(
            relations.begin(),
            relations.end(),
            [&](const auto& r)
            {
                return (r.first == actor && r.second == target) ||
                       (r.first == target && r.second == actor);
            }
        );
        if (found == relations.end())
        {
            relations.push_back({actor, target});
            found = std::prev(relations.end());
        }
        found->atWar = true;
        found->allied = false;
        found->trading = false;
        found->overlord = {};
        found->tributary = {};
        found->firstOpinion = found->secondOpinion = -100;
        return true;
    }
    std::string_view diplomaticResultText(DiplomaticResult result) noexcept
    {
        switch (result)
        {
        case DiplomaticResult::OutOfRange:
            return "Outside diplomatic range (45 degrees).";
        case DiplomaticResult::Success:
            return "Diplomatic action completed.";
        case DiplomaticResult::InvalidRealm:
            return "This realm no longer exists.";
        case DiplomaticResult::Self:
            return "Choose another realm for diplomacy.";
        case DiplomaticResult::Already:
            return "This agreement is already in effect.";
        case DiplomaticResult::AtWar:
            return "Establish peace first.";
        case DiplomaticResult::InsufficientGold:
            return "Not enough gold in your realm treasury.";
        case DiplomaticResult::InvalidGift:
            return "A gift must contain a positive amount of gold.";
        case DiplomaticResult::SubjectConflict:
            return "This would conflict with an existing tributary "
                   "relationship.";
        case DiplomaticResult::TreasuryOverflow:
            return "The receiving treasury cannot accept that amount.";
        }
        return "Unknown diplomatic result.";
    }
} // namespace Paladin
