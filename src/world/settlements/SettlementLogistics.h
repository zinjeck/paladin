#pragma once

#include "core/StrongId.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <span>
#include <string>
#include <vector>

namespace Paladin
{
struct InventoryIdTag;
using InventoryId = StrongId<InventoryIdTag>;
enum class InventoryKind
{
    Groundpile,
    Keep,
    Stockpile,
    Workplace,
    Construction
};

struct ResourceAmount
{
    std::string resource;
    int amount = 0;
};

struct SettlementInventory
{
    InventoryId id;
    InventoryKind kind = InventoryKind::Groundpile;
    SettlementObjectId objectId;
    ConstructionSiteId siteId;
    SettlementObjectFootprint footprint;
    int capacity = 0;
    double createdMinute = 0;
    std::vector<ResourceAmount> goods;
    int amount(std::string_view resource) const;
    int used() const;
};

struct HaulReservation
{
    CitizenId citizen;
    InventoryId source;
    InventoryId destination;
    std::string resource;
    int amount = 0;
    bool pickedUp = false;
};

// Physical goods and reservations share one settlement-owned authority.
// No API retains pointers into the object, inventory, or citizen vectors.
class SettlementLogistics
{
  public:
    void synchronize(const SettlementObjectState& objects, double minute);
    std::span<const SettlementInventory> inventories() const
    {
        return inventories_;
    }
    const SettlementInventory* inventory(InventoryId id) const;
    InventoryId forObject(SettlementObjectId id) const;
    InventoryId forSite(ConstructionSiteId id) const;
    InventoryId drop(
        SettlementTilePosition tile,
        std::string_view resource,
        int amount,
        double minute
    );
    int available(InventoryId id, std::string_view resource) const;
    int freeSpace(InventoryId id) const;
    bool add(
        InventoryId id,
        std::string_view resource,
        int amount,
        double minute = 0
    );
    bool consumeCarriedUnit(CitizenId citizen);
    bool founded() const
    {
        return foundingGoodsGranted_;
    }
    bool reserve(
        CitizenId citizen,
        InventoryId source,
        InventoryId destination,
        std::string_view resource,
        int amount
    );
    bool pickUp(CitizenId citizen);
    bool deliver(CitizenId citizen);
    const HaulReservation* reservation(CitizenId citizen) const;
    void release(CitizenId citizen);
    void consumeSite(ConstructionSiteId id);
    double total(std::string_view resource) const;
    std::uint64_t version() const
    {
        return version_;
    }

  private:
    SettlementInventory* edit(InventoryId id);
    void change(
        SettlementInventory& inventory,
        std::string_view resource,
        int amount
    );
    std::vector<SettlementInventory> inventories_;
    std::vector<HaulReservation> reservations_;
    IdGenerator<InventoryId> ids_;
    std::uint64_t objectVersion_ = ~std::uint64_t(0);
    std::uint64_t version_ = 0;
    bool foundingGoodsGranted_ = false;
};
} // namespace Paladin
