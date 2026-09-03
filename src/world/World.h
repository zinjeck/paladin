#pragma once

#include "core/EntityRegistry.h"
#include "core/StrongId.h"

#include "world/Army.h"
#include "world/Polity.h"
#include "world/Settlement.h"
#include "world/WorldPosition.h"
#include "world/WorldTime.h"
#include "world/WorldGrid.h"

#include <cstddef>

namespace Paladin
{
    class World
    {
    public:
        World();
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        void tick(double deltaSeconds);


        // ====================================================
        // Entity creation
        // ====================================================

        [[nodiscard]]
        WorldGrid& grid() noexcept;
        
        [[nodiscard]]
        const WorldGrid& grid() const noexcept;

        [[nodiscard]]
        SettlementId createSettlement(
            WorldPosition position = {}
        );

        [[nodiscard]]
        PolityId createPolity();

        [[nodiscard]]
        ArmyId createArmy(
            WorldPosition position = {}
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
            WorldPosition position
        ) noexcept;


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
            WorldPosition position
        ) noexcept;


        // ====================================================
        // Counts
        // ====================================================

        [[nodiscard]]
        std::size_t settlementCount() const noexcept;

        [[nodiscard]]
        std::size_t polityCount() const noexcept;

        [[nodiscard]]
        std::size_t armyCount() const noexcept;


    private:
        WorldTime time_;
    
        EntityRegistry<
            Settlement,
            SettlementId
        > settlements_;

        EntityRegistry<
            Polity,
            PolityId
        > polities_;

        EntityRegistry<
            Army,
            ArmyId
        > armies_;
    };
}