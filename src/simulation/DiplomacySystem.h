#pragma once
#include "world/Diplomacy.h"
#include <string_view>
namespace Paladin
{
    class World;
    enum class DiplomaticResult { Success, InvalidRealm, Self, Already, AtWar,
        InsufficientGold, InvalidGift, SubjectConflict, TreasuryOverflow, OutOfRange };
    class DiplomacySystem
    {
    public:
        // Angular geographic range, independent of flat/globe camera and wrapping.
        static constexpr double RangeRadians = .7853981633974483; // 45 degrees
        static bool inRange(const World&, RealmId, RealmId);
        static double distance(const World&, RealmId, RealmId);
        static int opinion(const World&, RealmId observer, RealmId target);
        static Money suggestedGift(const World&, RealmId actor, RealmId target);
        // Initial deterministic diplomacy: agreements take effect immediately.
        // Negotiation, AI acceptance and combat resolution are separate systems.
        static DiplomaticResult apply(World&, RealmId actor, RealmId target,
                                      DiplomaticAction, Money gift = 1000);
    };
    std::string_view diplomaticResultText(DiplomaticResult) noexcept;
}
