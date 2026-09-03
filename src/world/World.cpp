#include "world/World.h"

namespace Paladin
{
    World::World() = default;

    World::~World() = default;


    void World::tick(double deltaSeconds)
    {
        // Future strategic simulation.
        //
        // This layer will eventually coordinate things such as:
        //
        // - settlement progression
        // - polity AI
        // - diplomacy
        // - strategic army movement
        // - warfare
        // - trade
        // - world-scale economic systems
        //
        // None of those systems will require rendering.

        (void)deltaSeconds;
    }


    SettlementId World::createSettlement()
    {
        return settlements_.create();
    }


    PolityId World::createPolity()
    {
        return polities_.create();
    }


    ArmyId World::createArmy()
    {
        return armies_.create();
    }


    Settlement* World::settlement(
        SettlementId id
    ) noexcept
    {
        return settlements_.find(id);
    }


    const Settlement* World::settlement(
        SettlementId id
    ) const noexcept
    {
        return settlements_.find(id);
    }


    Polity* World::polity(
        PolityId id
    ) noexcept
    {
        return polities_.find(id);
    }


    const Polity* World::polity(
        PolityId id
    ) const noexcept
    {
        return polities_.find(id);
    }


    Army* World::army(
        ArmyId id
    ) noexcept
    {
        return armies_.find(id);
    }


    const Army* World::army(
        ArmyId id
    ) const noexcept
    {
        return armies_.find(id);
    }


    std::size_t World::settlementCount() const noexcept
    {
        return settlements_.size();
    }


    std::size_t World::polityCount() const noexcept
    {
        return polities_.size();
    }


    std::size_t World::armyCount() const noexcept
    {
        return armies_.size();
    }
}