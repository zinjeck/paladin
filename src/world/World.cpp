#include "world/World.h"
#include "world/TerrainType.h"
#include "world/generation/WorldGenerator.h"

namespace Paladin
{
    World::World()
        : World(WorldGenerationSettings{})
    {
    }


    World::World(
        const WorldGenerationSettings& generationSettings
    )
        : generationSeed_(generationSettings.seed),
          grid_(
              generationSettings.width,
              generationSettings.height
          )
    {
        WorldGenerator{}.generate(
            grid_,
            generationSettings
        );
    }

    World::~World() = default;


    void World::tick(double deltaSeconds)
    {
        time_.advance(deltaSeconds);
    
        // Future strategic systems will update here.
    }

    WorldTime& World::time() noexcept
    {
        return time_;
    }
    
    
    const WorldTime& World::time() const noexcept
    {
        return time_;
    }

    WorldGrid& World::grid() noexcept
    {
        return grid_;
    }
    
    
    const WorldGrid& World::grid() const noexcept
    {
        return grid_;
    }


    std::uint64_t World::generationSeed() const noexcept
    {
        return generationSeed_;
    }


    std::span<const Settlement> World::settlements() const noexcept
    {
        return settlements_.entities();
    }

    // ========================================================
    // Creation
    // ========================================================

    SettlementId World::createSettlement(
        WorldPosition position
    )
    {
        return settlements_.create(position);
    }


    PolityId World::createPolity()
    {
        return polities_.create();
    }


    ArmyId World::createArmy(
        WorldPosition position
    )
    {
        return armies_.create(position);
    }


    bool World::canFoundSettlementAt(
        WorldPosition position
    ) const noexcept
    {
        const WorldTile* tile =
            grid_.tile({
                position.x,
                position.y
            });

        if (!tile || tile->terrain != TerrainType::Land)
        {
            return false;
        }

        for (const Settlement& settlement : settlements_.entities())
        {
            if (settlement.position() == position)
            {
                return false;
            }
        }

        return true;
    }


    SettlementId World::foundSettlement(
        WorldPosition position,
        PolityId ownerPolityId
    )
    {
        if (
            !canFoundSettlementAt(position) ||
            !polities_.contains(ownerPolityId)
        )
        {
            return {};
        }

        const SettlementId settlementId =
            settlements_.create(position);

        Settlement* foundedSettlement =
            settlements_.find(settlementId);

        foundedSettlement->setOwnerPolity(ownerPolityId);

        return settlementId;
    }


    // ========================================================
    // Lookup
    // ========================================================

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


    // ========================================================
    // Settlement relationships
    // ========================================================

    bool World::assignSettlementToPolity(
        SettlementId settlementId,
        PolityId polityId
    ) noexcept
    {
        Settlement* targetSettlement =
            settlements_.find(settlementId);

        const Polity* targetPolity =
            polities_.find(polityId);

        if (!targetSettlement || !targetPolity)
        {
            return false;
        }

        targetSettlement->setOwnerPolity(polityId);

        return true;
    }


    bool World::makeSettlementIndependent(
        SettlementId settlementId
    ) noexcept
    {
        Settlement* targetSettlement =
            settlements_.find(settlementId);

        if (!targetSettlement)
        {
            return false;
        }

        targetSettlement->setOwnerPolity(
            PolityId{}
        );

        return true;
    }


    bool World::setSettlementPosition(
        SettlementId settlementId,
        WorldPosition position
    ) noexcept
    {
        Settlement* targetSettlement =
            settlements_.find(settlementId);

        if (!targetSettlement)
        {
            return false;
        }

        targetSettlement->setPosition(position);

        return true;
    }


    // ========================================================
    // Army relationships
    // ========================================================

    bool World::assignArmyToPolity(
        ArmyId armyId,
        PolityId polityId
    ) noexcept
    {
        Army* targetArmy =
            armies_.find(armyId);

        const Polity* targetPolity =
            polities_.find(polityId);

        if (!targetArmy || !targetPolity)
        {
            return false;
        }

        targetArmy->setOwnerPolity(polityId);

        return true;
    }


    bool World::makeArmyIndependent(
        ArmyId armyId
    ) noexcept
    {
        Army* targetArmy =
            armies_.find(armyId);

        if (!targetArmy)
        {
            return false;
        }

        targetArmy->setOwnerPolity(
            PolityId{}
        );

        return true;
    }


    bool World::setArmyPosition(
        ArmyId armyId,
        WorldPosition position
    ) noexcept
    {
        Army* targetArmy =
            armies_.find(armyId);

        if (!targetArmy)
        {
            return false;
        }

        targetArmy->setPosition(position);

        return true;
    }


    // ========================================================
    // Counts
    // ========================================================

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
