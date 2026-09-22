#pragma once
#include "world/settlements/SettlementMap.h"
#include <algorithm>
#include <string>
#include "world/settlements/citizens/SettlementCitizenState.h"

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

        static std::string feedback(const SettlementMap& map,
                                    const SettlementCitizenState& people,
                                    const SettlementTradeOrder& order)
        {
            const auto* depot = map.logistics.inventory(map.logistics.forObject(order.depot));
            if (!depot) { return "Depot storage is unavailable."; }
            const int target = map.trade.exportTarget(order.depot, order.resource);
            const int held = std::min(target, depot->amount(order.resource));
            const int incoming = map.logistics.incoming(depot->id, order.resource);
            const std::string progress = std::to_string(held) + "/" + std::to_string(target);
            if (held >= target) { return "Ready to load: " + progress + "."; }
            if (incoming > 0)
            {
                return "Collecting " + progress + " | " + std::to_string(incoming) + " on the way.";
            }
            const auto workplace = map.employment().forObject(order.depot);
            if (!workplace || !map.employment().employed(workplace, people))
            { return "Assign workers to this trade depot."; }
            if (map.logistics.receivable(depot->id, order.resource) <= 0)
            { return "Export storage full; finish or clear other batches."; }
            if (available(map, order.resource) == 0)
            { return "Waiting for unreserved city stock (" + progress + ")."; }
            if (order.collectionIssue == DepotCollectionIssue::NoPath)
            { return "No worker path: check storage/depot entrances."; }
            return "Waiting for depot workers (" + progress + ").";
        }

        static std::string status(const SettlementMap& map,
                                  const SettlementCitizenState& people,
                                  const SettlementTradeOrder& order, double minute)
        {
            const auto inventoryId = map.logistics.forObject(order.depot);
            const auto* inventory = map.logistics.inventory(inventoryId);
            if (!inventory) { return "Depot no longer exists"; }
            const int staged = std::min(order.quantity, inventory->amount(order.resource));
            if (staged >= order.quantity) { return "Batch ready; loading caravan"; }
            const auto job = map.employment().forObject(order.depot);
            if (!job || !map.employment().employed(job, people))
            { return "Assign a depot worker to collect"; }
            const int incoming = map.logistics.incoming(inventoryId, order.resource);
            const std::string progress = std::to_string(staged) + "/" +
                                        std::to_string(order.quantity);
            if (incoming > 0) { return "Fetching goods | " + progress + " ready"; }
            if (!map.activities.policy.isWorkTime(minute))
            { return "Off shift | collection resumes at work"; }
            if (!map.logistics.receivable(inventoryId, order.resource))
            { return "Export counter full | waiting for space"; }
            if (!available(map, order.resource))
            { return "Waiting for unreserved city stock"; }
            if (order.collectionIssue == DepotCollectionIssue::NoPath)
            { return "No usable route to stock or depot"; }
            return "Sale secured | " + progress + " ready";
        }

        static void report(SettlementMap& map, SettlementObjectId depot,
                           std::string_view resource, DepotCollectionIssue issue)
        {
            for (auto& order : map.trade.orders)
            {
                if (order.enabled && order.collectionAuthorized &&
                    order.direction == TradeDirection::Export &&
                    order.depot == depot && order.resource == resource)
                { order.collectionIssue = issue; }
            }
        }
    };
}
