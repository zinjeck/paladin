#pragma once

#include "core/EntityRegistry.h"
#include "core/StrongId.h"

#include "world/Army.h"
#include "world/Polity.h"
#include "world/Settlement.h"

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


        // ----------------------------------------------------
        // Entity creation
        // ----------------------------------------------------

        [[nodiscard]]
        SettlementId createSettlement();

        [[nodiscard]]
        PolityId createPolity();

        [[nodiscard]]
        ArmyId createArmy();


        // ----------------------------------------------------
        // Entity lookup
        // ----------------------------------------------------

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


        // ----------------------------------------------------
        // Counts
        // ----------------------------------------------------

        [[nodiscard]]
        std::size_t settlementCount() const noexcept;

        [[nodiscard]]
        std::size_t polityCount() const noexcept;

        [[nodiscard]]
        std::size_t armyCount() const noexcept;


    private:
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