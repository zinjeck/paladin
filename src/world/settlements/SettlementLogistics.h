#pragma once

#include "core/StrongId.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <span>
#include <string>
#include <unordered_map>
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
        Market,
        Construction,
        Home,
        TradeDepot,
        TradeImports
    };

    inline bool countsAsCityStorage(InventoryKind kind)
    {
        return kind == InventoryKind::Keep ||
               kind == InventoryKind::Stockpile ||
               kind == InventoryKind::Workplace ||
               kind == InventoryKind::Market ||
               kind == InventoryKind::TradeDepot ||
               kind == InventoryKind::TradeImports;
    }
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
        std::vector<ResourceAmount> resourceLimits;
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
        InventoryId importsForObject(SettlementObjectId id) const;
        InventoryId forSite(ConstructionSiteId id) const;
        InventoryId drop(
            SettlementTilePosition tile,
            std::string_view resource,
            int amount,
            double minute
        );
        int available(InventoryId id, std::string_view resource) const;
        int incoming(InventoryId id, std::string_view resource) const;
        bool importsMaySupply(const SettlementInventory& source,
                              InventoryKind destination) const
        {
            if (source.kind != InventoryKind::TradeImports) { return true; }
            if (destination == InventoryKind::TradeDepot) { return false; }
            return destination != InventoryKind::TradeImports;
        }
        int freeSpace(InventoryId id) const;
        int receivable(InventoryId id, std::string_view resource) const;
        bool add(
            InventoryId id,
            std::string_view resource,
            int amount,
            double minute = 0
        );
        bool consumeCarriedUnit(CitizenId citizen);
        int convert(
            InventoryId id,
            std::string_view input,
            std::string_view output,
            int requested,
            double minute
        );
        // Ingredients and military packs are not ordinary wholesale outputs.
        bool mayExport(
            const SettlementObjectState&,
            const SettlementInventory&,
            std::string_view resource
        ) const;

        bool consumeAvailable(
            InventoryId,
            std::string_view resource,
            int amount
        );
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
        int moveAvailable(
            InventoryId source,
            InventoryId destination,
            std::string_view resource,
            int requested
        );
        bool deliver(CitizenId citizen);
        const HaulReservation* reservation(CitizenId citizen) const;
        void release(CitizenId citizen);
        void consumeSite(ConstructionSiteId id);
        double total(std::string_view resource) const;
        double storedTotal(std::string_view resource) const;
        bool canEat(std::string_view resource) const;
        std::uint64_t version() const
        {
            return version_;
        }

    private:
        using InventoryClaims = std::unordered_map<
            InventoryId, std::vector<CitizenId>, StrongIdHash>;
        static std::span<const CitizenId> claimsFor(const InventoryClaims&, InventoryId);
        static void unindexClaim(InventoryClaims&, InventoryId, CitizenId);
        void synchronizeIndexes() const;
        mutable std::size_t indexedSize_ = 0;
        mutable std::unordered_map<InventoryId, std::size_t, StrongIdHash>
            inventoryIndex_;
        mutable std::
            unordered_map<SettlementObjectId, InventoryId, StrongIdHash>
                objectIndex_;
        mutable std::
            unordered_map<SettlementObjectId, InventoryId, StrongIdHash>
                importIndex_;
        mutable std::
            unordered_map<ConstructionSiteId, InventoryId, StrongIdHash>
                siteIndex_;
        SettlementInventory* edit(InventoryId id);
        void change(
            SettlementInventory& inventory,
            std::string_view resource,
            int amount
        );
        std::vector<SettlementInventory> inventories_;
        std::vector<HaulReservation> reservations_;
        // Index canonical claims by person and endpoint. These contain IDs,
        // never a second copy of cargo quantities or reservation state.
        std::unordered_map<CitizenId, std::size_t, StrongIdHash> reservationIndex_;
        InventoryClaims sourceClaims_, destinationClaims_;
        IdGenerator<InventoryId> ids_;
        std::uint64_t objectVersion_ = ~std::uint64_t(0);
        std::uint64_t version_ = 0;
        bool foundingGoodsGranted_ = false;
    };
} // namespace Paladin
