#pragma once
#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include <cstddef>
#include <unordered_map>

namespace Paladin
{
    class World;
    class Army;
    class SettlementMap;
    struct SettlementCitizen;
    enum class MilitaryResult
    {
        Success, InvalidUnit, NotOwned, NoBarracksEmployee, ReturnHome,
        EmptyUnit, InvalidDestination, NoLandRoute, UnitLimit
    };
    const char* militaryResultText(MilitaryResult result) noexcept;

    class MilitarySystem
    {
    public:
        // No independent soldier count: synchronize actual staffed barracks.
        static void synchronize(World&, double minute);
        static void tick(World&, double minute, double elapsed);
        static ArmyId createUnit(World&, RealmId actor, SettlementId home);
        static MilitaryResult resizeUnit(World&, RealmId actor, ArmyId, int delta);
        static MilitaryResult disbandUnit(World&, RealmId actor, ArmyId);
        static MilitaryResult orderMove(World&, RealmId actor, ArmyId, WorldTilePosition);
        static MilitaryResult recruit(World&, RealmId actor, SettlementId, int delta);
        static std::size_t available(const World&, SettlementId) noexcept;
        static bool presentAt(const World&, const Army&, SettlementId) noexcept;
        static bool canOrganize(const World&, const Army&) noexcept;
        static constexpr int RationsPerSoldier = 6; // three nominal days
        static constexpr std::size_t MaximumUnitsPerRealm = 256;
    private:
        static SettlementMap* map(World&, SettlementId);
        static SettlementCitizen* person(World&, const class Soldier&);
        using PersonnelIndex = std::unordered_map<SoldierId, SettlementCitizen*, StrongIdHash>;
        static void setDeployed(World&, Army&, bool, double minute, const PersonnelIndex* = nullptr);
        static void resupply(World&, Army&, double minute);
        static void returnSurplus(World&, Army&, double minute);
    };
}
