#include "world/settlements/ResourceStockpile.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Paladin
{
    namespace
    {
        auto findEntry(
            std::vector<StockpileEntry>& entries,
            std::string_view resourceId
        )
        {
            return std::lower_bound(
                entries.begin(),
                entries.end(),
                resourceId,
                [](
                    const StockpileEntry& entry,
                    std::string_view id
                )
                {
                    return entry.resourceId < id;
                }
            );
        }

        auto findEntry(
            const std::vector<StockpileEntry>& entries,
            std::string_view resourceId
        )
        {
            return std::lower_bound(
                entries.begin(),
                entries.end(),
                resourceId,
                [](
                    const StockpileEntry& entry,
                    std::string_view id
                )
                {
                    return entry.resourceId < id;
                }
            );
        }
    }

    double ResourceStockpile::amount(
        std::string_view resourceId
    ) const noexcept
    {
        const auto iterator = findEntry(entries_, resourceId);

        if (
            iterator == entries_.end() ||
            iterator->resourceId != resourceId
        )
        {
            return 0.0;
        }

        return iterator->amount;
    }

    bool ResourceStockpile::setAmount(
        std::string resourceId,
        double newAmount
    )
    {
        if (
            resourceId.empty() ||
            !std::isfinite(newAmount) ||
            newAmount < 0.0
        )
        {
            return false;
        }

        const auto iterator = findEntry(entries_, resourceId);

        if (
            iterator != entries_.end() &&
            iterator->resourceId == resourceId
        )
        {
            iterator->amount = newAmount;
            return true;
        }

        entries_.insert(
            iterator,
            {std::move(resourceId), newAmount}
        );

        return true;
    }

    bool ResourceStockpile::addAmount(
        std::string_view resourceId,
        double addedAmount
    )
    {
        if (!std::isfinite(addedAmount))
        {
            return false;
        }

        const double currentAmount = amount(resourceId);
        const double newAmount = currentAmount + addedAmount;

        if (!std::isfinite(newAmount) || newAmount < 0.0)
        {
            return false;
        }

        return setAmount(
            std::string(resourceId),
            newAmount
        );
    }

    void ResourceStockpile::clear() noexcept
    {
        entries_.clear();
    }

    std::span<const StockpileEntry>
    ResourceStockpile::entries() const noexcept
    {
        return entries_;
    }
}
