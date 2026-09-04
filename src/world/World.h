#pragma once

#include "core/EntityRegistry.h"
#include "core/StrongId.h"

#include "world/Army.h"
#include "world/Culture.h"
#include "world/FoundingIdentity.h"
#include "world/Polity.h"
#include "world/Settlement.h"
#include "world/WorldTilePosition.h"
#include "world/WorldTime.h"
#include "world/WorldGrid.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/territory/TerritoryFoundationPolicy.h"
#include "world/territory/TerritoryMap.h"

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
        explicit World(
            const WorldGenerationSettings& generationSettings
        );

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
        SettlementId createSettlement(
            WorldTilePosition position = {}
        );

        [[nodiscard]]
        PolityId createPolity();

        [[nodiscard]]
        CultureId createCulture(
            std::string name
        );

        [[nodiscard]]
        ArmyId createArmy(
            WorldTilePosition position = {}
        );

        [[nodiscard]]
        bool canFoundSettlementAt(
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        bool canFoundSettlementAt(
            WorldTilePosition position,
            PolityId ownerPolityId
        ) const noexcept;

        [[nodiscard]]
        SettlementId foundSettlement(
            WorldTilePosition position,
            PolityId ownerPolityId
        );

        [[nodiscard]]
        SettlementId foundSettlement(
            WorldTilePosition position,
            PolityId ownerPolityId,
            const SettlementFoundationProfile& foundationProfile
        );

        [[nodiscard]]
        SettlementId foundCapitalSettlement(
            WorldTilePosition position,
            PolityId ownerPolityId,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        SettlementId foundCapitalSettlement(
            WorldTilePosition position,
            PolityId ownerPolityId,
            const FoundingIdentity& identity,
            const SettlementFoundationProfile& foundationProfile
        );


        // ====================================================
        // Entity lookup
        // ====================================================

        [[nodiscard]]
        Settlement* settlement(
            SettlementId id
        ) noexcept;

        [[nodiscard]]
        const Settlement* settlement(
            SettlementId id
        ) const noexcept;


        [[nodiscard]]
        Polity* polity(
            PolityId id
        ) noexcept;

        [[nodiscard]]
        const Polity* polity(
            PolityId id
        ) const noexcept;

        [[nodiscard]]
        Culture* culture(
            CultureId id
        ) noexcept;

        [[nodiscard]]
        const Culture* culture(
            CultureId id
        ) const noexcept;


        [[nodiscard]]
        Army* army(
            ArmyId id
        ) noexcept;

        [[nodiscard]]
        const Army* army(
            ArmyId id
        ) const noexcept;

        [[nodiscard]]
        WorldTime& time() noexcept;
        
        [[nodiscard]]
        const WorldTime& time() const noexcept;

        [[nodiscard]]
        WorldGrid& grid() noexcept;
        
        [[nodiscard]]
        const WorldGrid& grid() const noexcept;

        [[nodiscard]]
        const TerritoryMap& territory() const noexcept;

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
        std::span<const Polity> polities() const noexcept;

        // ====================================================
        // Settlement relationships
        // ====================================================

        bool assignSettlementToPolity(
            SettlementId settlementId,
            PolityId polityId
        ) noexcept;

        bool makeSettlementIndependent(
            SettlementId settlementId
        ) noexcept;

        bool setSettlementPosition(
            SettlementId settlementId,
            WorldTilePosition position
        ) noexcept;

        [[nodiscard]]
        bool renameSettlement(
            SettlementId settlementId,
            std::string name
        );

        [[nodiscard]]
        bool editPolityIdentity(
            PolityId polityId,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        bool relocateSoleCapital(
            PolityId polityId,
            WorldTilePosition position
        );


        // ====================================================
        // Army relationships
        // ====================================================

        bool assignArmyToPolity(
            ArmyId armyId,
            PolityId polityId
        ) noexcept;

        bool makeArmyIndependent(
            ArmyId armyId
        ) noexcept;

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
        std::size_t polityCount() const noexcept;

        [[nodiscard]]
        std::size_t cultureCount() const noexcept;

        [[nodiscard]]
        std::size_t armyCount() const noexcept;


    private:
        WorldTime time_;
        std::uint64_t generationSeed_ = 0;
        WorldGrid grid_;
        TerritoryMap territory_;
        TerritoryFoundationPolicy territoryFoundationPolicy_;
    
        EntityRegistry<
            Settlement,
            SettlementId
        > settlements_;

        EntityRegistry<
            Polity,
            PolityId
        > polities_;

        EntityRegistry<
            Culture,
            CultureId
        > cultures_;

        EntityRegistry<
            Army,
            ArmyId
        > armies_;
    };
}
