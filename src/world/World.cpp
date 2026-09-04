#include "world/World.h"
#include "world/TerrainType.h"
#include "world/PolityOrigin.h"
#include "world/generation/WorldGenerator.h"
#include "world/territory/TerritoryFoundationSystem.h"

#include <utility>

namespace Paladin
{
    World::World()
        : World(
              WorldGenerationSettings{},
              defaultTerritoryFoundationPolicy()
          )
    {
    }


    World::World(
        const WorldGenerationSettings& generationSettings
    )
        : World(
              generationSettings,
              defaultTerritoryFoundationPolicy()
          )
    {
    }


    World::World(
        const WorldGenerationSettings& generationSettings,
        TerritoryFoundationPolicy territoryFoundationPolicy
    )
        : generationSeed_(generationSettings.seed),
          grid_(
              generationSettings.width,
              generationSettings.height
          ),
          territory_(
              generationSettings.width,
              generationSettings.height
          ),
          territoryFoundationPolicy_(
              std::move(territoryFoundationPolicy)
          )
    {
        WorldGenerator{}.generate(
            grid_,
            generationSettings
        );
    }

    World::~World() = default;


    void World::advanceTime(
        std::uint64_t gameMinutes
    ) noexcept
    {
        time_.advanceMinutes(gameMinutes);
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


    const TerritoryMap& World::territory() const noexcept
    {
        return territory_;
    }


    const TerritoryFoundationPolicy&
    World::territoryFoundationPolicy() const noexcept
    {
        return territoryFoundationPolicy_;
    }


    std::uint64_t World::generationSeed() const noexcept
    {
        return generationSeed_;
    }


    std::span<const Settlement> World::settlements() const noexcept
    {
        return settlements_.entities();
    }


    std::span<Settlement> World::settlements() noexcept
    {
        return settlements_.entities();
    }


    std::span<const Culture> World::cultures() const noexcept
    {
        return cultures_.entities();
    }


    std::span<const Polity> World::polities() const noexcept
    {
        return polities_.entities();
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
        return canFoundSettlementAt(position, {});
    }


    bool World::canFoundSettlementAt(
        WorldPosition position,
        PolityId ownerPolityId
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

        if (
            territoryFoundationPolicy_.settlementRegionWidth <= 0 ||
            territoryFoundationPolicy_.settlementRegionHeight <= 0
        )
        {
            return false;
        }

        const WorldTilePosition regionTopLeft{
            position.x
                - territoryFoundationPolicy_
                    .settlementRegionWidth / 2,
            position.y
                - territoryFoundationPolicy_
                    .settlementRegionHeight / 2
        };

        const WorldTilePosition regionBottomRight{
            regionTopLeft.x
                + territoryFoundationPolicy_
                    .settlementRegionWidth - 1,
            regionTopLeft.y
                + territoryFoundationPolicy_
                    .settlementRegionHeight - 1
        };

        if (
            !grid_.isValidPosition(regionTopLeft) ||
            !grid_.isValidPosition(regionBottomRight)
        )
        {
            return false;
        }

        const PolityId existingController =
            territory_.controllerAt({
                position.x,
                position.y
            });

        if (
            existingController.isValid() &&
            existingController != ownerPolityId
        )
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
        return foundSettlement(
            position,
            ownerPolityId,
            defaultSettlementFoundationProfile()
        );
    }


    SettlementId World::foundSettlement(
        WorldPosition position,
        PolityId ownerPolityId,
        const SettlementFoundationProfile& foundationProfile
    )
    {
        if (
            !canFoundSettlementAt(position, ownerPolityId) ||
            !polities_.contains(ownerPolityId)
        )
        {
            return {};
        }

        const SettlementId settlementId = settlements_.create(
            position,
            std::string{},
            ownerPolityId,
            CultureId{},
            foundationProfile
        );

        static_cast<void>(
            TerritoryFoundationSystem{}
                .establishSettlementTerritory(
                    grid_,
                    territory_,
                    position,
                    ownerPolityId,
                    territoryFoundationPolicy_,
                    territoryFoundationPolicy_
                        .settlementBorderlandTraversalBudget
                )
        );

        return settlementId;
    }


    SettlementId World::foundCapitalSettlement(
        WorldPosition position,
        PolityId ownerPolityId,
        const FoundingIdentity& identity
    )
    {
        return foundCapitalSettlement(
            position,
            ownerPolityId,
            identity,
            defaultSettlementFoundationProfile()
        );
    }


    SettlementId World::foundCapitalSettlement(
        WorldPosition position,
        PolityId ownerPolityId,
        const FoundingIdentity& identity,
        const SettlementFoundationProfile& foundationProfile
    )
    {
        Polity* ownerPolity = polities_.find(ownerPolityId);

        if (
            !ownerPolity ||
            ownerPolity->capitalSettlementId().isValid() ||
            !canFoundSettlementAt(position, ownerPolityId) ||
            !isValidFoundingName(identity.polityName) ||
            !isValidFoundingName(identity.cultureName) ||
            !isValidFoundingName(identity.capitalName) ||
            !isKnownPolityOrigin(identity.polityOriginId) ||
            !identity.flag.isValid()
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
                cultureId,
                foundationProfile
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
            std::move(originId),
            identity.flag
        );

        static_cast<void>(
            TerritoryFoundationSystem{}
                .establishSettlementTerritory(
                    grid_,
                    territory_,
                    position,
                    ownerPolityId,
                    territoryFoundationPolicy_,
                    territoryFoundationPolicy_
                        .capitalBorderlandTraversalBudget
                )
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


    bool World::renameSettlement(
        SettlementId settlementId,
        std::string name
    )
    {
        Settlement* targetSettlement = settlements_.find(settlementId);

        if (!targetSettlement || !isValidFoundingName(name))
        {
            return false;
        }

        targetSettlement->setName(trimFoundingName(name));
        return true;
    }


    bool World::editPolityIdentity(
        PolityId polityId,
        const FoundingIdentity& identity
    )
    {
        Polity* targetPolity = polities_.find(polityId);

        if (
            !targetPolity ||
            !isValidFoundingName(identity.polityName) ||
            !isValidFoundingName(identity.cultureName) ||
            !isKnownPolityOrigin(identity.polityOriginId) ||
            !identity.flag.isValid()
        )
        {
            return false;
        }

        Culture* primaryCulture = cultures_.find(
            targetPolity->primaryCultureId()
        );

        if (!primaryCulture)
        {
            return false;
        }

        primaryCulture->setName(
            trimFoundingName(identity.cultureName)
        );

        targetPolity->editIdentity(
            identity.mapColor,
            trimFoundingName(identity.polityName),
            identity.polityOriginId,
            identity.flag
        );

        return true;
    }


    bool World::relocateSoleCapital(
        PolityId polityId,
        WorldPosition position
    )
    {
        Polity* targetPolity = polities_.find(polityId);

        if (!targetPolity || !canFoundSettlementAt(position, polityId))
        {
            return false;
        }

        Settlement* capital = settlements_.find(
            targetPolity->capitalSettlementId()
        );

        if (!capital)
        {
            return false;
        }

        std::size_t ownedSettlementCount = 0;

        for (const Settlement& settlement : settlements_.entities())
        {
            if (settlement.ownerPolityId() == polityId)
            {
                ++ownedSettlementCount;
            }
        }

        // This setup operation deliberately cannot erase territory belonging
        // to additional settlements. A later colony/capital-transfer system
        // can provide territory provenance for established polities.
        if (ownedSettlementCount != 1)
        {
            return false;
        }

        territory_.clearController(polityId);
        capital->setPosition(position);

        static_cast<void>(
            TerritoryFoundationSystem{}
                .establishSettlementTerritory(
                    grid_,
                    territory_,
                    position,
                    polityId,
                    territoryFoundationPolicy_,
                    territoryFoundationPolicy_
                        .capitalBorderlandTraversalBudget
                )
        );

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
