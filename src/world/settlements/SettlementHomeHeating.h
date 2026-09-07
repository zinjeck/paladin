#pragma once
#include "world/Season.h"
#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <unordered_map>
#include <unordered_set>

namespace Paladin
{
    class SettlementHomeHeating
    {
    public:
        // The burn fraction persists through season changes. Empty houses do
        // not burn fuel. A delivered log supplies 12 hours, or 6 in winter.
        void advance(
            SettlementLogistics& logistics,
            const std::unordered_set<SettlementObjectId, StrongIdHash>&
                occupied,
            double minute,
            double elapsed
        )
        {
            if (elapsed <= 0)
            {
                return;
            }
            std::erase_if(
                fires_,
                [&](const auto& p) { return !logistics.forObject(p.first); }
            );
            for (const auto& inventory : logistics.inventories())
            {
                if (inventory.kind != InventoryKind::Home)
                {
                    continue;
                }
                auto& fire = fires_[inventory.objectId];
                fire.heated = false;
                fire.coldFraction = 0;
                if (!occupied.contains(inventory.objectId))
                {
                    continue;
                }
                const double duration =
                    seasonAtMinute(minute) == Season::Winter ? 360 : 720;
                double remaining = elapsed / duration;
                while (remaining > 1e-9)
                {
                    if (fire.fuel <= 1e-9 &&
                        !logistics.consumeAvailable(
                            inventory.id,
                            SettlementResourceTypes::Lumber,
                            1
                        ))
                    {
                        break;
                    }
                    if (fire.fuel <= 1e-9)
                    {
                        fire.fuel = 1;
                        ++lumberBurned_;
                    }
                    const double used = std::min(remaining, fire.fuel);
                    remaining -= used;
                    fire.fuel -= used;
                }
                fire.coldFraction =
                    std::clamp(remaining * duration / elapsed, 0.0, 1.0);
                fire.heated = fire.coldFraction < 1e-9;
            }
        }
        bool heated(SettlementObjectId id) const
        {
            const auto it = fires_.find(id);
            return it != fires_.end() && it->second.heated;
        }
        std::uint64_t lumberBurned() const
        {
            return lumberBurned_;
        }
        double coldFraction(SettlementObjectId id) const
        {
            const auto it = fires_.find(id);
            return it == fires_.end() ? 1 : it->second.coldFraction;
        }

    private:
        std::uint64_t lumberBurned_ = 0;
        struct Fire
        {
            double fuel = 0, coldFraction = 1;
            bool heated = false;
        };
        std::unordered_map<SettlementObjectId, Fire, StrongIdHash> fires_;
    };
} // namespace Paladin
