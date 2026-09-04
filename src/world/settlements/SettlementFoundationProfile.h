#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementEconomy.h"
#include "world/settlements/SettlementPopulation.h"
#include "world/settlements/SettlementSimulationTier.h"

#include <cstdint>
#include <vector>

namespace Paladin
{
    struct SettlementFoundationProfile
    {
        std::uint64_t initialPopulation = 100;
        DemographicRates demographicRates;
        std::vector<StockpileEntry> initialResources;
        std::vector<ResourceFlowRate> resourceFlowRates;
        SettlementSimulationTier initialSimulationTier =
            SettlementSimulationTier::Inactive;
    };

    [[nodiscard]]
    SettlementFoundationProfile defaultSettlementFoundationProfile();
}
