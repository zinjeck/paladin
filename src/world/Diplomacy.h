#pragma once
#include "core/StrongId.h"
#include "world/settlements/SettlementCommerce.h"
#include <vector>

namespace Paladin
{
    enum class DiplomaticAction { Alliance, Gift, Tribute, Trade, War, Peace };
    struct DiplomaticRelation
    {
        RealmId first, second;
        bool allied = false, trading = false, atWar = false;
        // The subject may have only one overlord. This never changes land ownership.
        RealmId overlord, tributary;
        // Opinions are directional even though treaties are shared.
        int firstOpinion = 0, secondOpinion = 0;
    };
    struct DiplomacyState
    {
        std::vector<DiplomaticRelation> relations;
        const DiplomaticRelation* between(RealmId a, RealmId b) const noexcept
        {
            for (const auto& r : relations)
                if ((r.first == a && r.second == b) || (r.first == b && r.second == a)) return &r;
            return nullptr;
        }
        RealmId overlordOf(RealmId subject) const noexcept
        {
            for (const auto& r : relations) if (r.tributary == subject) return r.overlord;
            return {};
        }
    };
}
