#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/SettlementEmploymentState.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>

namespace Paladin
{
int SettlementInventory::amount(std::string_view resource) const
{
    for (const auto& entry : goods)
    {
        if (entry.resource == resource)
        {
            return entry.amount;
        }
    }
    return 0;
}
int SettlementInventory::used() const
{
    int result = 0;
    for (const auto& entry : goods)
    {
        result += entry.amount;
    }
    return result;
}
const SettlementInventory* SettlementLogistics::inventory(InventoryId id) const
{
    for (const auto& entry : inventories_)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}
SettlementInventory* SettlementLogistics::edit(InventoryId id)
{
    for (auto& entry : inventories_)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}
InventoryId SettlementLogistics::forObject(SettlementObjectId id) const
{
    for (const auto& entry : inventories_)
    {
        if (id && entry.objectId == id)
        {
            return entry.id;
        }
    }
    return {};
}
InventoryId SettlementLogistics::forSite(ConstructionSiteId id) const
{
    for (const auto& entry : inventories_)
    {
        if (id && entry.siteId == id)
        {
            return entry.id;
        }
    }
    return {};
}
void SettlementLogistics::change(
    SettlementInventory& entry,
    std::string_view resource,
    int amount
)
{
    for (auto& goods : entry.goods)
    {
        if (goods.resource == resource)
        {
            goods.amount += amount;
            ++version_;
            return;
        }
    }
    entry.goods.push_back({std::string(resource), amount});
    ++version_;
}
int SettlementLogistics::available(
    InventoryId id,
    std::string_view resource
) const
{
    const auto* entry = inventory(id);
    int amount = entry ? entry->amount(resource) : 0;
    for (const auto& claim : reservations_)
    {
        if (claim.source == id && claim.resource == resource && !claim.pickedUp)
        {
            amount -= claim.amount;
        }
    }
    return std::max(0, amount);
}
int SettlementLogistics::freeSpace(InventoryId id) const
{
    const auto* entry = inventory(id);
    int amount = entry ? entry->capacity - entry->used() : 0;
    for (const auto& claim : reservations_)
    {
        if (claim.destination == id)
        {
            amount -= claim.amount;
        }
    }
    return std::max(0, amount);
}
bool SettlementLogistics::add(
    InventoryId id,
    std::string_view resource,
    int amount,
    double minute
)
{
    if (amount <= 0 || !SettlementResourceCatalog::definition(resource) ||
        freeSpace(id) < amount)
    {
        return false;
    }
    auto* entry = edit(id);
    if (!entry)
    {
        return false;
    }
    if (entry->used() == 0)
    {
        entry->createdMinute = minute;
    }
    change(*entry, resource, amount);
    return true;
}
InventoryId SettlementLogistics::drop(
    SettlementTilePosition tile,
    std::string_view resource,
    int amount,
    double minute
)
{
    InventoryId result;
    if (amount <= 0 || !SettlementResourceCatalog::definition(resource))
    {
        return result;
    }
    for (auto& entry : inventories_)
    {
        if (entry.kind != InventoryKind::Groundpile ||
            entry.footprint.topLeft != tile)
        {
            continue;
        }
        const int deposited = std::min(amount, freeSpace(entry.id));
        if (deposited <= 0)
        {
            continue;
        }
        change(entry, resource, deposited);
        result = entry.id;
        amount -= deposited;
        if (amount == 0)
        {
            return result;
        }
    }
    while (amount > 0)
    {
        const int deposited = std::min(amount, 100);
        result = ids_.generate();
        inventories_.push_back(
            {result,
             InventoryKind::Groundpile,
             {},
             {},
             {tile, 1, 1},
             100,
             minute,
             {{std::string(resource), deposited}}}
        );
        amount -= deposited;
        ++version_;
    }
    return result;
}
const HaulReservation* SettlementLogistics::reservation(CitizenId citizen) const
{
    for (const auto& claim : reservations_)
    {
        if (claim.citizen == citizen)
        {
            return &claim;
        }
    }
    return nullptr;
}
bool SettlementLogistics::reserve(
    CitizenId citizen,
    InventoryId source,
    InventoryId destination,
    std::string_view resource,
    int amount
)
{
    if (!citizen || reservation(citizen) || amount <= 0 ||
        source == destination || available(source, resource) < amount ||
        (destination && freeSpace(destination) < amount))
    {
        return false;
    }
    reservations_.push_back(
        {citizen, source, destination, std::string(resource), amount}
    );
    ++version_;
    return true;
}
bool SettlementLogistics::pickUp(CitizenId citizen)
{
    for (auto& claim : reservations_)
    {
        if (claim.citizen != citizen || claim.pickedUp)
        {
            continue;
        }
        auto* entry = edit(claim.source);
        if (!entry || entry->amount(claim.resource) < claim.amount)
        {
            return false;
        }
        change(*entry, claim.resource, -claim.amount);
        claim.pickedUp = true;
        if (entry->kind == InventoryKind::Groundpile && entry->used() == 0)
        {
            const auto id = entry->id;
            std::erase_if(
                inventories_,
                [id](const auto& i) { return i.id == id; }
            );
        }
        return true;
    }
    return false;
}
bool SettlementLogistics::consumeCarriedUnit(CitizenId citizen)
{
    for (auto& claim : reservations_)
    {
        if (claim.citizen != citizen)
        {
            continue;
        }
        if (!claim.pickedUp || claim.amount <= 0)
        {
            return false;
        }
        --claim.amount;
        if (claim.amount == 0)
        {
            release(citizen);
        }
        ++version_;
        return true;
    }
    return true;
}
bool SettlementLogistics::deliver(CitizenId citizen)
{
    const auto* claim = reservation(citizen);
    if (!claim || !claim->pickedUp)
    {
        return false;
    }
    auto* entry = edit(claim->destination);
    if (!entry || entry->capacity - entry->used() < claim->amount)
    {
        return false;
    }
    change(*entry, claim->resource, claim->amount);
    release(citizen);
    return true;
}
void SettlementLogistics::release(CitizenId citizen)
{
    if (std::erase_if(
            reservations_,
            [citizen](const auto& r) { return r.citizen == citizen; }
        ))
    {
        ++version_;
    }
}
void SettlementLogistics::consumeSite(ConstructionSiteId id)
{
    if (auto* entry = edit(forSite(id)))
    {
        entry->goods.clear();
        ++version_;
    }
}
double SettlementLogistics::total(std::string_view resource) const
{
    double result = 0;
    for (const auto& entry : inventories_)
    {
        result += entry.amount(resource);
    }
    return result;
}
void SettlementLogistics::synchronize(
    const SettlementObjectState& objects,
    double minute
)
{
    if (objectVersion_ == objects.navigationVersion())
    {
        return;
    }
    objectVersion_ = objects.navigationVersion();
    std::vector<SettlementInventory> removed;
    std::erase_if(
        inventories_,
        [&](const auto& entry)
        {
            if ((entry.objectId && !objects.completedObject(entry.objectId)) ||
                (entry.siteId && !objects.constructionSite(entry.siteId)))
            {
                removed.push_back(entry);
                return true;
            }
            return false;
        }
    );
    for (const auto& entry : removed)
    {
        for (const auto& goods : entry.goods)
        {
            drop(entry.footprint.topLeft, goods.resource, goods.amount, minute);
        }
    }
    for (const auto& object : objects.completedObjects())
    {
        if (forObject(object.id))
        {
            continue;
        }
        const bool keep =
            object.objectTypeId == SettlementObjectTypes::CityKeep;
        const bool stockpile =
            object.objectTypeId == SettlementObjectTypes::Stockpile;
        if (!keep && !workplaceDefinition(object.objectTypeId))
        {
            continue;
        }
        const auto id = ids_.generate();
        inventories_.push_back(
            {id,
             keep        ? InventoryKind::Keep
             : stockpile ? InventoryKind::Stockpile
                         : InventoryKind::Workplace,
             object.id,
             {},
             object.footprint,
             keep ? 100
                  : workplaceDefinition(object.objectTypeId)->storageCapacity}
        );
        if (keep && !foundingGoodsGranted_)
        {
            foundingGoodsGranted_ = true;
            add(id, SettlementResourceTypes::Lumber, 40);
            add(id, SettlementResourceTypes::Stone, 40);
            add(id, SettlementResourceTypes::Fish, 20);
        }
    }
    for (const auto& site : objects.constructionSites())
    {
        if (forSite(site.id))
        {
            continue;
        }
        int capacity = 0;
        std::vector<ResourceAmount> goods;
        for (const auto& cost : site.resourceDeliveries)
        {
            capacity += int(cost.requiredAmount);
            goods.push_back({cost.resourceId, int(cost.deliveredAmount)});
        }
        inventories_.push_back(
            {ids_.generate(),
             InventoryKind::Construction,
             {},
             site.id,
             site.footprint,
             capacity,
             minute,
             std::move(goods)}
        );
    }
    ++version_;
}
} // namespace Paladin
