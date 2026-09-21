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
        Success,
        InvalidUnit,
        NotOwned,
        NoBarracksEmployee,
        ReturnHome,
        EmptyUnit,
        InvalidDestination,
        NoLandRoute,
        UnitLimit,
        PersonnelOrigin,
        InBattle
    };
    const char* militaryResultText(MilitaryResult result) noexcept;

    class MilitarySystem
    {
    public:
        // No independent soldier count: synchronize actual staffed barracks.
        static void synchronize(World&, double minute, bool force = true);
        static void tick(World&, double minute, double elapsed);
        static std::uint64_t rosterRebuilds(const World&) noexcept;
        static std::uint64_t personnelUpdates(const World&) noexcept;
        // Removes canonical people and their payroll/reservations, never
        // abstract manpower.
        static std::size_t applyBattleCasualties(World&, ArmyId, std::size_t);
        // Only AI strategic settlements may materialize a bounded cadre from
        // their existing aggregate civilians. Every enlisted person then uses
        // the same canonical Soldier/Army pipeline as the player's force.
        static ArmyId maintainStrategicGarrison(
            World&,
            RealmId,
            SettlementId,
            int target,
            int changeLimit = 8
        );
        static ArmyId createUnit(
            World&,
            RealmId actor,
            SettlementId home,
            bool garrison = false
        );
        static MilitaryResult setGarrison(
            World&,
            RealmId,
            ArmyId,
            SettlementId
        );
        static std::size_t reserves(const World&, RealmId) noexcept;
        static MilitaryResult resizeUnit(
            World&,
            RealmId actor,
            ArmyId,
            int delta
        );
        static MilitaryResult disbandUnit(World&, RealmId actor, ArmyId);
        static MilitaryResult orderMove(
            World&,
            RealmId actor,
            ArmyId,
            WorldTilePosition
        );
        static MilitaryResult recruit(
            World&,
            RealmId actor,
            SettlementId,
            int delta
        );
        static std::size_t available(const World&, SettlementId) noexcept;
        static bool presentAt(const World&, const Army&, SettlementId) noexcept;
        static bool canOrganize(const World&, const Army&) noexcept;
        static SettlementId stationAt(const World&, const Army&) noexcept;
        static std::size_t releasable(const World&, const Army&) noexcept;
        static constexpr int RationsPerSoldier = 6; // three nominal days
        static constexpr std::size_t MaximumUnitsPerRealm = 256;

    private:
        static SettlementMap* map(World&, SettlementId);
        static SettlementCitizen* person(World&, const class Soldier&);
        using PersonnelIndex =
            std::unordered_map<SoldierId, SettlementCitizen*, StrongIdHash>;
        static void setDeployed(
            World&,
            Army&,
            bool,
            double minute,
            const PersonnelIndex* = nullptr
        );
        static void resupply(World&, Army&, double minute);
        static void returnSurplus(World&, Army&, double minute);
        static bool restoreEmployment(World&, const class Soldier&);
    };
} // namespace Paladin
