#include "simulation/DiplomacySystem.h"
#include "world/World.h"
#include <limits>
namespace Paladin
{
    DiplomaticResult DiplomacySystem::apply(World& world, RealmId actor, RealmId target,
                                           DiplomaticAction action, Money gift)
    {
        auto* a = world.realm(actor); auto* b = world.realm(target);
        if (!a || !b) return DiplomaticResult::InvalidRealm;
        if (actor == target) return DiplomaticResult::Self;
        auto& state = world.diplomacy();
        auto relation = state.between(actor, target) ? *state.between(actor, target) : DiplomaticRelation{actor,target};
        if (action == DiplomaticAction::Gift)
        {
            if (gift <= 0) return DiplomaticResult::InvalidGift;
            if (!a->treasury || !b->treasury || a->treasury->balance < gift) return DiplomaticResult::InsufficientGold;
            if (b->treasury->balance > std::numeric_limits<Money>::max() - gift) return DiplomaticResult::TreasuryOverflow;
            // Preflight both sides before mutation; no gold is created or destroyed.
            a->treasury->moneyEconomyStarted = true;
            a->treasury->balance -= gift; b->treasury->balance += gift;
            b->treasury->moneyEconomyStarted = true;
            return DiplomaticResult::Success;
        }
        switch (action)
        {
        case DiplomaticAction::Alliance:
            if (relation.atWar) return DiplomaticResult::AtWar;
            if (relation.allied) return DiplomaticResult::Already;
            relation.allied = true; break;
        case DiplomaticAction::Trade:
            if (relation.atWar) return DiplomaticResult::AtWar;
            if (relation.trading) return DiplomaticResult::Already;
            relation.trading = true; break;
        case DiplomaticAction::Tribute:
            if (relation.overlord == actor && relation.tributary == target)
            { relation.overlord = {}; relation.tributary = {}; break; }
            if (relation.atWar) return DiplomaticResult::AtWar;
            if (state.overlordOf(target)) return DiplomaticResult::SubjectConflict;
            // Reject self-cycles and indirect cycles in the subject graph.
            {
                auto parent=actor;
                std::size_t remaining=world.realms().size()+1;
                while (parent)
                {
                    if (parent==target || remaining--==0) return DiplomaticResult::SubjectConflict;
                    parent=state.overlordOf(parent);
                }
            }
            relation.overlord = actor; relation.tributary = target; break;
        case DiplomaticAction::War:
            if (relation.atWar) return DiplomaticResult::Already;
            relation.atWar = true; relation.allied = relation.trading = false;
            relation.overlord = relation.tributary = {}; break;
        case DiplomaticAction::Peace:
            if (!relation.atWar) return DiplomaticResult::Already;
            relation.atWar = false; break;
        case DiplomaticAction::Gift: break;
        }
        for (auto& r : state.relations)
            if ((r.first == actor && r.second == target) || (r.first == target && r.second == actor))
            { r = relation; return DiplomaticResult::Success; }
        state.relations.push_back(relation);
        return DiplomaticResult::Success;
    }
    std::string_view diplomaticResultText(DiplomaticResult result) noexcept
    {
        switch (result)
        {
        case DiplomaticResult::Success: return "Diplomatic action completed.";
        case DiplomaticResult::InvalidRealm: return "This realm no longer exists.";
        case DiplomaticResult::Self: return "Choose another realm for diplomacy.";
        case DiplomaticResult::Already: return "This agreement is already in effect.";
        case DiplomaticResult::AtWar: return "Establish peace first.";
        case DiplomaticResult::InsufficientGold: return "Not enough gold in your realm treasury.";
        case DiplomaticResult::InvalidGift: return "A gift must contain a positive amount of gold.";
        case DiplomaticResult::SubjectConflict: return "This would conflict with an existing tributary relationship.";
        case DiplomaticResult::TreasuryOverflow: return "The receiving treasury cannot accept that amount.";
        }
        return "Unknown diplomatic result.";
    }
}
