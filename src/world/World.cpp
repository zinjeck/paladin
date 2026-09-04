#include "world/World.h"
#include "world/TerrainType.h"
#include "world/PolityOrigin.h"
#include "world/generation/WorldGenerator.h"

#include <utility>

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


    std::span<const Culture> World::cultures() const noexcept
    {
        return cultures_.entities();
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


    CultureId World::createCulture(
        std::string name
    )
    {
        if (!isValidFoundingName(name))
        {
            return {};
        }

        return cultures_.create(
            trimFoundingName(name)
        );
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


    SettlementId World::foundCapitalSettlement(
        WorldPosition position,
        PolityId ownerPolityId,
        const FoundingIdentity& identity
    )
    {
        Polity* ownerPolity = polities_.find(ownerPolityId);

        if (
            !ownerPolity ||
            ownerPolity->capitalSettlementId().isValid() ||
            !canFoundSettlementAt(position) ||
            !isValidFoundingName(identity.polityName) ||
            !isValidFoundingName(identity.cultureName) ||
            !isValidFoundingName(identity.capitalName) ||
            !isKnownPolityOrigin(identity.polityOriginId)
        )
        {
            return {};
        }

        std::string polityName =
            trimFoundingName(identity.polityName);

        std::string cultureName =
            trimFoundingName(identity.cultureName);

        std::string capitalName =
            trimFoundingName(identity.capitalName);

        std::string originId = identity.polityOriginId;

        const CultureId cultureId =
            cultures_.create(
                std::move(cultureName)
            );

        SettlementId settlementId;

        try
        {
            settlementId = settlements_.create(
                position,
                std::move(capitalName),
                ownerPolityId,
                cultureId
            );
        }
        catch (...)
        {
            cultures_.erase(cultureId);
            throw;
        }

        ownerPolity->establishCapital(
            settlementId,
            cultureId,
            identity.mapColor,
            std::move(polityName),
            std::move(originId)
        );

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


    Culture* World::culture(
        CultureId id
    ) noexcept
    {
        return cultures_.find(id);
    }


    const Culture* World::culture(
        CultureId id
    ) const noexcept
    {
        return cultures_.find(id);
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


    std::size_t World::cultureCount() const noexcept
    {
        return cultures_.size();
    }


    std::size_t World::armyCount() const noexcept
    {
        return armies_.size();
    }
}
