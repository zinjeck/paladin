#pragma once

#include "core/EntityRegistry.h"
#include "core/StrongId.h"

#include "world/Army.h"
#include "world/Culture.h"
#include "world/FoundingIdentity.h"
#include "world/Realm.h"
#include "world/Settlement.h"
#include "world/WorldGrid.h"
#include "world/WorldTilePosition.h"
#include "world/WorldTime.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/territory/TerritoryFoundationPolicy.h"
#include "world/territory/TerritoryMap.h"
#include "world/territory/TribalInfluenceMap.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Paladin
{
    class World
    {
    public:
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
        bool canFoundSettlementAt(WorldTilePosition position) const noexcept;

        [[nodiscard]]
        bool canFoundSettlementAt(
            WorldTilePosition position,
            RealmId ownerRealmId
        ) const noexcept;

        [[nodiscard]]
        bool canFoundAdditionalSettlementAt(
            WorldTilePosition position,
            RealmId owner
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


    private:
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
    };
} // namespace Paladin
