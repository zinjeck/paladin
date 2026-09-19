#pragma once
#include "core/StrongId.h"
#include "simulation/MilitarySystem.h"
#include "world/WorldTilePosition.h"
#include <optional>
#include <cstddef>

namespace Paladin
{
    class World;
    struct BattleEncounter
    {
        ArmyId player, enemy;
        WorldTilePosition tile;
    };
    struct BattleResult
    {
        bool resolved = false;
        ArmyId winner;
        std::size_t playerLosses = 0, enemyLosses = 0;
    };
    // Physical encounters and rosters are simulation state. SDL, a camera and a
    // temporary battlefield never own a second copy of the world's soldiers.
    class BattleSystem
    {
    public:
        static MilitaryResult orderAttack(World&, RealmId, ArmyId, ArmyId);
        static void updatePursuit(World&);
        static void detectContacts(World&);
        static std::optional<BattleEncounter> pendingFor(const World&, RealmId);
        static bool valid(const World&, const BattleEncounter&) noexcept;
        static bool retreat(World&, RealmId, const BattleEncounter&);
        static BattleResult simulate(World&, RealmId, const BattleEncounter&);
    private:
        static void stop(class Army&);
        static void release(World&, ArmyId, ArmyId);
        static double strength(const World&, const class Army&);
    };
}
