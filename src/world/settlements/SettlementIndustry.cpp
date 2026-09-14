#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>
#include <string>
namespace Paladin {
bool produceIndustry(SettlementMap& map, SettlementObjectId objectId,
                     int workers, double minute, double elapsed)
{
    const auto* object = map.objectState().completedObject(objectId);
    if (!object) return false;
    const auto type = object->objectTypeId;
    if (type != "wheat_farm" && type != "bakery" &&
        type != "army_supply_depot" && type != "barracks") return false;
    if (workers <= 0 || elapsed <= 0) return true;
    const auto destinationId = map.logistics.forObject(objectId);
    if (!map.logistics.inventory(destinationId)) return true;
    // Bounded stock targets prevent a processor draining city stores in a tick.
    const int target = std::min(60, std::max(4, workers * 8));
    const auto buy = [&](std::string_view resource, bool depotOnly) {
        const auto* current = map.logistics.inventory(destinationId);
        if (!current || current->amount(resource) >= target) return;
        const auto destinationCopy = *current;
        for (const auto& source : map.logistics.inventories()) {
            if (source.id == destinationId || source.kind == InventoryKind::Construction ||
                source.kind == InventoryKind::Home) continue;
            const auto* seller = map.objectState().completedObject(source.objectId);
            if (depotOnly && (!seller || seller->objectTypeId != "army_supply_depot")) continue;
            if (!depotOnly && seller && (seller->objectTypeId == "barracks" ||
                seller->objectTypeId == "army_supply_depot")) continue;
            const int amount = std::min({target-current->amount(resource),
                map.logistics.available(source.id,resource),
                map.logistics.receivable(destinationId,resource),
                map.commerce.affordableTradeUnits(source,destinationCopy,target)});
            if (amount <= 0) continue;
            const auto sourceCopy = source;
            // Funds and unreserved goods are checked before either transfer.
            // No interleaved simulation mutation occurs between these calls.
            if (map.commerce.buyGoods(sourceCopy,destinationCopy,amount)) {
                const int moved = map.logistics.moveAvailable(sourceCopy.id,destinationId,resource,amount);
                map.commerce.recordFlow(sourceCopy.id,destinationId,resource,moved);
            }
            if (map.logistics.inventory(destinationId)->amount(resource) >= target) break;
        }
    };
    if (type == "barracks") { buy("rations",true); return true; }
    if (type == "bakery") buy("wheat",false);
    if (type == "army_supply_depot") {
        const auto* stored = map.logistics.inventory(destinationId);
        int foodCount = 0;
        for (const auto& food : SettlementResourceCatalog::definitions())
            if (food.edible && !food.emergencyOnly) foodCount += stored->amount(food.id);
        if (foodCount < target && stored->amount("rations") < target)
            for (const auto& food : SettlementResourceCatalog::definitions())
                if (food.edible && !food.emergencyOnly) {
                    buy(food.id,false);
                    if (map.logistics.inventory(destinationId)->amount(food.id) > 0) break;
                }
    }
    const char* output = type == "wheat_farm" ? "wheat" : type == "bakery" ? "bread" : "rations";
    const auto* stored = map.logistics.inventory(destinationId);
    std::string input;
    if (type == "bakery") input = "wheat";
    if (type == "army_supply_depot")
        for (const auto& food : SettlementResourceCatalog::definitions())
            if (food.edible && !food.emergencyOnly && map.logistics.available(destinationId,food.id)>0) {
                input = food.id; break;
            }
    if (type != "wheat_farm" && (input.empty() || map.logistics.available(destinationId,input)<=0)) return true;
    if (stored->amount(output) >= target || map.logistics.receivable(destinationId,output) <= 0) return true;
    const double rate = workers / (type == "wheat_farm" ? 45.0 : 30.0);
    int amount = std::min({target-stored->amount(output),map.logistics.receivable(destinationId,output),
        int(map.objectState().accrueProduction(objectId,elapsed*rate))});
    if (!input.empty()) amount = std::min(amount,map.logistics.available(destinationId,input));
    if (amount<=0) return true;
    if (!input.empty()) {
        if (!map.logistics.consumeAvailable(destinationId,input,amount)) return true;
        map.commerce.recordConsumption(input,amount);
    }
    if (map.logistics.add(destinationId,output,amount,minute)) map.commerce.recordProduction(output,amount);
    return true;
}
}
