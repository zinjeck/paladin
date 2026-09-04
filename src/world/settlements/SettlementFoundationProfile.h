#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementPopulation.h"

#include <cstdint>
#include <vector>

namespace Paladin
{
    struct SettlementFoundationProfile
    {
        std::uint64_t initialPopulation = 100;
        DemographicRates demographicRates;
        std::vector<StockpileEntry> initialResources;
    };

    [[nodiscard]]
    SettlementFoundationProfile defaultSettlementFoundationProfile();
}
