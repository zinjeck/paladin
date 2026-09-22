#pragma once
#include "world/settlements/SettlementMap.h"

namespace Paladin
{
    // Depot orders are city-wide jobs, not the stockpile's optional local haul.
    // Collection, feedback and coarse simulation share these same limits.
    struct DepotCollection
    {
        static bool source(const SettlementMap& map,
                           const SettlementInventory& inventory,
                           std::string_view resource)
        {
            return (inventory.kind == InventoryKind::Stockpile ||
                    inventory.kind == InventoryKind::Workplace ||
                    inventory.kind == InventoryKind::Groundpile) &&
                   map.logistics.mayExport(map.objectState(), inventory, resource);
        }

        static int available(const SettlementMap& map, std::string_view resource)
        {
            int count = 0;
            for (const auto& inventory : map.logistics.inventories())
            {
                if (source(map, inventory, resource))
                {
                    count += std::min(1000000 - count,
                        map.logistics.available(inventory.id, resource));
                }
            }
            return count;
        }

        static int needed(const SettlementMap& map, const SettlementInventory& depot,
                          std::string_view resource)
        {
            return std::max(0, map.trade.exportTarget(depot.objectId, resource) -
                depot.amount(resource) - map.logistics.incoming(depot.id, resource));
        }

        static void report(SettlementMap& map, SettlementObjectId depot,
                           std::string_view resource, DepotCollectionIssue issue)
        {
            for (auto& order : map.trade.orders)
            {
                if (order.enabled && order.collectionAuthorized &&
                    order.depot == depot && order.resource == resource)
                { order.collectionIssue = issue; }
            }
        }
    };
}
