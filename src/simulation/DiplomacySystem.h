#pragma once
#include "world/Diplomacy.h"
#include <string_view>
namespace Paladin
{
    class World;
    enum class DiplomaticResult { Success, InvalidRealm, Self, Already, AtWar,
        InsufficientGold, InvalidGift, SubjectConflict, TreasuryOverflow };
    class DiplomacySystem
    {
    public:
        // Initial deterministic diplomacy: agreements take effect immediately.
        // Negotiation, AI acceptance and combat resolution are separate systems.
        static DiplomaticResult apply(World&, RealmId actor, RealmId target,
                                      DiplomaticAction, Money gift = 1000);
    };
    std::string_view diplomaticResultText(DiplomaticResult) noexcept;
}
