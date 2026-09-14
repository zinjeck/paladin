#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace Paladin
{
    bool isIndustry(std::string_view type) noexcept
    {
        return type == SettlementObjectTypes::WheatFarm || type == SettlementObjectTypes::Bakery ||
               type == SettlementObjectTypes::ArmySupplyDepot || type == SettlementObjectTypes::Barracks;
    }
    bool produceIndustry(SettlementMap& map, SettlementObjectId objectId,
                         int workers, double minute, double elapsed)
    {
        const auto* object = map.objectState().completedObject(objectId);
        if (!object || !isIndustry(object->objectTypeId)) return false;
        if (workers <= 0 || !std::isfinite(minute) || !std::isfinite(elapsed) || elapsed <= 0) return true;
        const auto type = object->objectTypeId;
        const auto destinationId = map.logistics.forObject(objectId);
        const auto* inventory = map.logistics.inventory(destinationId);
        if (!inventory) return true;
        const int capacity = inventory->capacity;
        // Keep room for output. Shared input targets, not one target per food.
        const int target = std::max(1, std::min({60, workers * 8, capacity / 2}));
        const auto buy = [&](std::string_view resource, int requested, bool depotOnly)
        {
            int wanted = requested;
            // No inventory creation during transfer: IDs/copies stay valid and
            // both reservations and cash are checked before committing.
            for (const auto& candidate : map.logistics.inventories())
            {
                if (wanted <= 0) break;
                if (candidate.id == destinationId || candidate.kind == InventoryKind::Construction ||
                    candidate.kind == InventoryKind::Home) continue;
                const auto* seller = map.objectState().completedObject(candidate.objectId);
                if (depotOnly && (!seller || seller->objectTypeId != SettlementObjectTypes::ArmySupplyDepot)) continue;
                if (!map.logistics.mayExport(map.objectState(), candidate, resource)) continue;
                const auto source = candidate;
                const auto destination = *map.logistics.inventory(destinationId);
                const int amount = std::min({wanted, map.logistics.available(source.id, resource),
                    map.logistics.receivable(destinationId, resource),
                    map.commerce.affordableTradeUnits(source, destination, wanted)});
                if (amount <= 0 || !map.commerce.buyGoods(source, destination, amount)) continue;
                const int moved = map.logistics.moveAvailable(source.id, destinationId, resource, amount);
                map.commerce.recordFlow(source.id, destinationId, resource, moved);
                wanted -= moved;
            }
        };
        if (type == SettlementObjectTypes::Barracks)
        {
            buy(SettlementResourceTypes::Rations,
                std::max(0, std::min(capacity, workers * 6) - inventory->amount(SettlementResourceTypes::Rations)), true);
            return true;
        }
        if (type == SettlementObjectTypes::Bakery)
            buy(SettlementResourceTypes::Wheat, std::max(0, target - inventory->amount(SettlementResourceTypes::Wheat)), false);
        if (type == SettlementObjectTypes::ArmySupplyDepot)
        {
            int food = 0;
            for (const auto& definition : SettlementResourceCatalog::definitions())
                if (definition.edible && !definition.emergencyOnly) food += inventory->amount(definition.id);
            int needed = std::max(0, target - food);
            for (const auto& definition : SettlementResourceCatalog::definitions())
            {
                if (!definition.edible || definition.emergencyOnly || needed <= 0) continue;
                const int before = inventory->amount(definition.id);
                buy(definition.id, needed, false);
                needed -= inventory->amount(definition.id) - before;
            }
        }
        inventory = map.logistics.inventory(destinationId);
        if (type == SettlementObjectTypes::WheatFarm)
        {
            const int ready = map.objectState().prepareGrainHarvest(objectId, minute);
            const int room = map.logistics.receivable(destinationId, SettlementResourceTypes::Wheat);
            if (ready <= 0 || room <= 0) return true;
            const int amount = std::min({ready, room, int(std::min(1000000.0,
                map.objectState().accrueProduction(objectId, elapsed * workers / 2.0)))});
            if (amount > 0 && map.logistics.add(destinationId, SettlementResourceTypes::Wheat, amount, minute))
            {
                map.objectState().takeGrainHarvest(objectId, amount, minute);
                map.commerce.recordProduction(SettlementResourceTypes::Wheat, amount);
            }
            return true;
        }
        const auto output = type == SettlementObjectTypes::Bakery ? SettlementResourceTypes::Bread : SettlementResourceTypes::Rations;
        std::string input;
        if (type == SettlementObjectTypes::Bakery) input = SettlementResourceTypes::Wheat;
        else for (const auto& definition : SettlementResourceCatalog::definitions())
            if (definition.edible && !definition.emergencyOnly && map.logistics.available(destinationId, definition.id) > 0)
            { input = definition.id; break; }
        if (input.empty() || map.logistics.available(destinationId, input) <= 0 || inventory->amount(output) >= target) return true;
        const int amount = std::min(target - inventory->amount(output), int(std::min(1000000.0,
            map.objectState().accrueProduction(objectId, elapsed * workers / 30.0))));
        const int produced = map.logistics.convert(destinationId, input, output, amount, minute);
        if (produced > 0)
        {
            map.commerce.recordNonMealConsumption(input, produced);
            map.commerce.recordProduction(output, produced);
        }
        return true;
    }
    void advanceInactiveIndustry(SettlementMap& map, const SettlementCitizenState& citizens, double minute, double elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed <= 0) return;
        // Apply the actual local work schedule, including midnight wrapping and
        // longitude. Calendar crop age continues through the night.
        const auto& policy = map.activities.policy;
        const double start = policy.shiftStartMinute, end = policy.shiftEndMinute;
        const double span = std::clamp(end - start, 0.0, 1440.0);
        const double fullDays = std::floor(elapsed / 1440.0);
        double workMinutes = fullDays * span;
        double cursor = minute + fullDays * 1440.0, remainder = elapsed - fullDays * 1440.0;
        while (remainder > 1e-9)
        {
            const double local = policy.localMinute(cursor);
            const double untilMidnight = 1440.0 - local;
            const double dt = std::min(remainder, untilMidnight);
            workMinutes += std::max(0.0, std::min(local + dt, end) - std::max(local, start));
            cursor += dt; remainder -= dt;
        }
        for (const auto& work : map.employment().workplaces())
        {
            if (!work.operational || !isIndustry(work.objectTypeId) || work.objectTypeId == SettlementObjectTypes::Barracks) continue;
            produceIndustry(map, work.objectId, int(map.employment().employed(work.id, citizens)), minute + elapsed, workMinutes);
        }
    }
}
