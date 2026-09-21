#pragma once

#include "core/EntityRegistry.h"
#include "core/StrongId.h"

#include "world/Army.h"
#include "world/Culture.h"
#include "world/Diplomacy.h"
#include "world/FoundingIdentity.h"
#include "world/Realm.h"
#include "world/Settlement.h"
#include "world/Soldier.h"
#include "world/WorldGrid.h"
#include "world/WorldRoad.h"
#include "world/WorldShipment.h"
#include "world/WorldTilePosition.h"
#include "world/WorldTime.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/territory/TerritoryFoundationPolicy.h"
#include "world/territory/TerritoryMap.h"
#include "world/territory/TribalInfluenceMap.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <vector>

namespace Paladin
{
    struct MarketProducer
    {
        RealmId realm;
        double dailyOutput = 0;
    };
    struct MarketPriceSample
    {
        double minute = 0, price = 0;
    };
    struct WorldResourceMarket
    {
        std::string resource;
        double price = 0, demand = 0, supply = 0, weight = 0;
        std::vector<MarketProducer> producers;
        std::deque<MarketPriceSample> history;
    };
    struct WorldMarketHistory
    {
        std::vector<WorldResourceMarket> resources, pending;
        std::size_t cityCursor = 0;
        double nextMinute = 0, sampleMinute = 0;
        bool collecting = false;
    };
    class World
    {
    public:
        WorldMarketHistory marketHistory;
        std::span<const WorldShipment> shipments() const noexcept
        {
            return shipments_;
        }
        const WorldShipment* shipment(ShipmentId id) const noexcept
        {
            for (const auto& route : shipments_)
            {
                if (route.id == id)
                {
                    return &route;
                }
            }
            return nullptr;
        }
        DiplomacyState& diplomacy() noexcept
        {
            return diplomacy_;
        }
        const DiplomacyState& diplomacy() const noexcept
        {
            return diplomacy_;
        }
        const Soldier* soldier(SoldierId id) const noexcept
        {
            return soldiers_.find(id);
        }
        std::span<const Soldier> soldiers() const noexcept
        {
            return soldiers_.entities();
        }
        World();
        explicit World(const WorldGenerationSettings& generationSettings);

        World(
            const WorldGenerationSettings& generationSettings,
            TerritoryFoundationPolicy territoryFoundationPolicy
        );
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        void advanceTime(std::uint64_t gameMinutes) noexcept;


        // ====================================================
        // Entity creation
        // ====================================================

        [[nodiscard]]
        SettlementId createSettlement(WorldTilePosition position = {});

        [[nodiscard]]
        RealmId createRealm();

        [[nodiscard]]
        CultureId createCulture(std::string name);

        [[nodiscard]]
        ArmyId createArmy(WorldTilePosition position = {});

        [[nodiscard]]
        WorldRoadId createWorldRoad(
            std::span<const WorldTilePosition> points,
            RealmId ownerRealmId = {}
        );

        [[nodiscard]]
        bool canFoundSettlementAt(WorldTilePosition position) const noexcept;

        [[nodiscard]]
        bool canFoundSettlementAt(
            WorldTilePosition position,
            RealmId ownerRealmId,
            SettlementKind kind = SettlementKind::City
        ) const noexcept;

        [[nodiscard]]
        bool canFoundAdditionalSettlementAt(
            WorldTilePosition position,
            RealmId owner,
            SettlementKind kind = SettlementKind::City
        ) const noexcept;

        SettlementId foundSettlement(
            WorldTilePosition position,
            RealmId ownerRealmId
        );

        [[nodiscard]]
        SettlementId foundSettlement(
            WorldTilePosition position,
            RealmId ownerRealmId,
            const SettlementFoundationProfile& foundationProfile
        );

        [[nodiscard]]
        SettlementId foundCapitalSettlement(
            WorldTilePosition position,
            RealmId ownerRealmId,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        SettlementId foundCapitalSettlement(
            WorldTilePosition position,
            RealmId ownerRealmId,
            const FoundingIdentity& identity,
            const SettlementFoundationProfile& foundationProfile
        );


        // ====================================================
        // Entity lookup
        // ====================================================

        [[nodiscard]]
        Settlement* settlement(SettlementId id) noexcept;

        [[nodiscard]]
        const Settlement* settlement(SettlementId id) const noexcept;


        [[nodiscard]]
        Realm* realm(RealmId id) noexcept;

        [[nodiscard]]
        const Realm* realm(RealmId id) const noexcept;

        [[nodiscard]]
        Culture* culture(CultureId id) noexcept;

        [[nodiscard]]
        const Culture* culture(CultureId id) const noexcept;


        [[nodiscard]]
        Army* army(ArmyId id) noexcept;

        [[nodiscard]]
        const Army* army(ArmyId id) const noexcept;

        [[nodiscard]]
        WorldRoad* worldRoad(WorldRoadId id) noexcept;

        [[nodiscard]]
        const WorldRoad* worldRoad(WorldRoadId id) const noexcept;

        [[nodiscard]]
        WorldTime& time() noexcept;

        [[nodiscard]]
        const WorldTime& time() const noexcept;

        [[nodiscard]]
        WorldGrid& grid() noexcept;

        [[nodiscard]]
        const WorldGrid& grid() const noexcept;

        // Discrete controller cells are civic sovereignty only once a realm's
        // origin is known. Tribal authority is intentionally separate and may
        // overlap between realms.
        [[nodiscard]]
        const TerritoryMap& territory() const noexcept;

        [[nodiscard]]
        const TribalInfluenceMap& tribalInfluence() const;

        [[nodiscard]]
        const TerritoryFoundationPolicy&
        territoryFoundationPolicy() const noexcept;

        [[nodiscard]]
        std::uint64_t generationSeed() const noexcept;

        [[nodiscard]]
        std::span<const Settlement> settlements() const noexcept;

        [[nodiscard]]
        std::span<Settlement> settlements() noexcept;

        [[nodiscard]]
        std::span<const Army> armies() const noexcept;

        [[nodiscard]]
        std::span<Army> armies() noexcept;

        [[nodiscard]]
        std::span<const WorldRoad> worldRoads() const noexcept;

        [[nodiscard]]
        std::span<WorldRoad> worldRoads() noexcept;

        [[nodiscard]]
        std::span<const Culture> cultures() const noexcept;

        [[nodiscard]]
        std::span<const Realm> realms() const noexcept;

        // ====================================================
        // Settlement relationships
        // ====================================================

        bool assignSettlementToRealm(
            SettlementId settlementId,
            RealmId realmId
        ) noexcept;

        bool makeSettlementIndependent(SettlementId settlementId) noexcept;

        bool setSettlementPosition(
            SettlementId settlementId,
            WorldTilePosition position
        ) noexcept;

        [[nodiscard]]
        bool renameSettlement(SettlementId settlementId, std::string name);

        [[nodiscard]]
        bool editRealmIdentity(
            RealmId realmId,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        bool relocateSoleCapital(RealmId realmId, WorldTilePosition position);


        // ====================================================
        // Army relationships
        // ====================================================

        bool assignArmyToRealm(ArmyId armyId, RealmId realmId) noexcept;

        bool makeArmyIndependent(ArmyId armyId) noexcept;

        bool setArmyPosition(
            ArmyId armyId,
            WorldTilePosition position
        ) noexcept;


        // ====================================================
        // World-road relationships
        // ====================================================

        bool assignWorldRoadToRealm(
            WorldRoadId roadId,
            RealmId realmId
        ) noexcept;

        bool makeWorldRoadIndependent(WorldRoadId roadId) noexcept;


        // ====================================================
        // Counts
        // ====================================================

        [[nodiscard]]
        std::size_t settlementCount() const noexcept;

        [[nodiscard]]
        std::size_t realmCount() const noexcept;

        [[nodiscard]]
        std::size_t cultureCount() const noexcept;

        [[nodiscard]]
        std::size_t armyCount() const noexcept;

        [[nodiscard]]
        std::size_t worldRoadCount() const noexcept;


    private:
        friend class MilitarySystem;
        std::uint64_t militaryRosterStamp_ = 0;
        std::uint64_t militaryRosterRebuilds_ = 0;
        std::uint64_t militaryPersonnelUpdates_ = 0;
        friend class BattleSystem;
        friend class WorldShipmentSystem;
        std::vector<WorldShipment> shipments_;
        IdGenerator<ShipmentId> shipmentIds_;
        void connectRealmTerritory(RealmId realmId);
        DiplomacyState diplomacy_;
        WorldTime time_;
        std::uint64_t generationSeed_ = 0;
        WorldGrid grid_;
        TerritoryMap territory_;
        mutable TribalInfluenceMap tribalInfluence_;
        TerritoryFoundationPolicy territoryFoundationPolicy_;

        EntityRegistry<Settlement, SettlementId> settlements_;

        EntityRegistry<Realm, RealmId> realms_;

        EntityRegistry<Culture, CultureId> cultures_;

        EntityRegistry<Army, ArmyId> armies_;
        EntityRegistry<Soldier, SoldierId> soldiers_;

        EntityRegistry<WorldRoad, WorldRoadId> worldRoads_;
    };
} // namespace Paladin
