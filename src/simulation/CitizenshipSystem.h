#pragma once
#include "core/StrongId.h"
#include "world/RealmLaws.h"
#include <vector>

namespace Paladin
{
    class World;
    struct SettlementCitizen;
    struct ImmigrantOrigin { SettlementId settlement; CultureId culture; };
    class CitizenshipSystem
    {
    public:
        // Bounded nearby source choices, nearest first. No global random
        // cultures or player settlements are used as immigrant origins.
        static std::vector<ImmigrantOrigin> nearbyOrigins(const World&, SettlementId);
        static void synchronize(World&);
        static bool research(World&, RealmId);
        static void assignImmigrants(World&, SettlementId, std::size_t first,
                                     const std::vector<ImmigrantOrigin>&);
        static void inherit(SettlementCitizen& child, const SettlementCitizen& mother,
                            const SettlementCitizen& father, CultureId dominant,
                            RealmId realm, SettlementId bornIn, CitizenshipRights);
    };
}
