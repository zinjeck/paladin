#pragma once
#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>

namespace Paladin
{
    // Named foods share one civilian reserve; rations remain military stores.
    inline double civilianFood(const ResourceStockpile& stock)
    {
        double total = 0;
        for (const auto& resource : SettlementResourceCatalog::definitions())
        {
            if (resource.edible && !resource.emergencyOnly)
            {
                total += stock.amount(resource.id);
            }
        }
        return total;
    }
    inline double militaryFood(
        const ResourceStockpile& stock,
        double population
    )
    {
        return stock.amount("rations") +
               std::max(0., civilianFood(stock) - 2 * population);
    }
} // namespace Paladin
