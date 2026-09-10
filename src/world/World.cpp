#include "world/World.h"
#include "world/RealmOrigin.h"
#include "world/TerrainType.h"
#include "world/generation/WorldGenerator.h"
#include "world/territory/TerritoryFoundationSystem.h"

#include <utility>

namespace Paladin
{
    World::World()
        : World(WorldGenerationSettings{}, defaultTerritoryFoundationPolicy())
    {
    }


    World::World(const WorldGenerationSettings& generationSettings)
        : World(generationSettings, defaultTerritoryFoundationPolicy())
    {
    }


    World::World(
        const WorldGenerationSettings& generationSettings,
        TerritoryFoundationPolicy territoryFoundationPolicy
    )
        : generationSeed_(generationSettings.seed),
          grid_(generationSettings.width, generationSettings.height),
          territory_(generationSettings.width, generationSettings.height),
          tribalInfluence_(generationSettings.width, generationSettings.height),
          territoryFoundationPolicy_(std::move(territoryFoundationPolicy))
    {
        WorldGenerator{}.generate(grid_, generationSettings);
    }

    World::~World() = default;


    void World::advanceTime(std::uint64_t gameMinutes) noexcept
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


    const TribalInfluenceMap& World::tribalInfluence() const
    {
        tribalInfluence_.synchronize(
            grid_,
            realms_.entities(),
            settlements_.entities(),
            territoryFoundationPolicy_.tribalInfluence
        );
        return tribalInfluence_;
    }


    const TerritoryFoundationPolicy& World::
        territoryFoundationPolicy() const noexcept
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


    std::span<const Realm> World::realms() const noexcept
    {
        return realms_.entities();
    }

    // ========================================================
    // Creation
    // ========================================================

    SettlementId World::createSettlement(WorldTilePosition position)
    {
        return settlements_.create(position);
    }


    RealmId World::createRealm()
    {
        return realms_.create();
    }


    CultureId World::createCulture(std::string name)
    {
        if (!isValidFoundingName(name))
        {
            return {};
        }

        return cultures_.create(trimFoundingName(name));
    }


    ArmyId World::createArmy(WorldTilePosition position)
    {
        return armies_.create(position);
    }


    bool World::canFoundSettlementAt(WorldTilePosition position) const noexcept
    {
        return canFoundSettlementAt(position, {});
    }


    bool World::canFoundSettlementAt(
        WorldTilePosition position,
        RealmId ownerRealmId
    ) const noexcept
    {
        const WorldTile* tile = grid_.tile({position.x, position.y});

        if (!tile || tile->terrain != TerrainType::Land ||
            tile->biome == BiomeType::Tundra || tile->biome == BiomeType::Polar)
        {
            return false;
        }

        if (territoryFoundationPolicy_.settlementRegionWidth <= 0 ||
            territoryFoundationPolicy_.settlementRegionHeight <= 0)
        {
            return false;
        }

        const WorldTilePosition regionTopLeft{
            position.x - territoryFoundationPolicy_.settlementRegionWidth / 2,
            position.y - territoryFoundationPolicy_.settlementRegionHeight / 2
        };

        const WorldTilePosition regionBottomRight{
            regionTopLeft.x + territoryFoundationPolicy_.settlementRegionWidth -
                1,
            regionTopLeft.y +
                territoryFoundationPolicy_.settlementRegionHeight - 1
        };

        if (!grid_.isValidPosition(regionTopLeft) ||
            !grid_.isValidPosition(regionBottomRight))
        {
            return false;
        }

        // Only civic sovereignty is binary. Tribal influence may overlap and
        // therefore never blocks founding through controllerAt().
        const RealmId existingController =
            territory_.controllerAt({position.x, position.y});

        if (existingController.isValid() && existingController != ownerRealmId)
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


    bool World::canFoundAdditionalSettlementAt(
        WorldTilePosition position,
        RealmId owner
    ) const noexcept
    {
        if (!canFoundSettlementAt(position, owner))
        {
            return false;
        }
        const auto* realm = realms_.find(owner);
        const auto* capital =
            realm ? settlements_.find(realm->capitalSettlementId()) : nullptr;
        if (!capital)
        {
            return false;
        }
        const auto dx = std::int64_t(position.x) - capital->position().x;
        const auto dy = std::int64_t(position.y) - capital->position().y;
        if (dx * dx + dy * dy > 48 * 48)
        {
            return false;
        }
        const int width = territoryFoundationPolicy_.settlementRegionWidth;
        const int height = territoryFoundationPolicy_.settlementRegionHeight;
        for (const auto& settlement : settlements_.entities())
        {
            if (std::abs(position.x - settlement.position().x) < width &&
                std::abs(position.y - settlement.position().y) < height)
            {
                return false;
            }
        }
        int water = 0;
        for (int y = position.y - height / 2;
             y < position.y - height / 2 + height;
             ++y)
        {
            for (int x = position.x - width / 2;
                 x < position.x - width / 2 + width;
                 ++x)
            {
                const auto controller = territory_.controllerAt({x, y});
                if (controller && controller != owner)
                {
                    return false;
                }
                if (grid_.tile({x, y})->terrain == TerrainType::Water)
                {
                    ++water;
                }
            }
        }
        return water * 2 < width * height;
    }

    SettlementId World::foundSettlement(
        WorldTilePosition position,
        RealmId ownerRealmId
    )
    {
        return foundSettlement(
            position,
            ownerRealmId,
            defaultSettlementFoundationProfile()
        );
    }


    SettlementId World::foundSettlement(
        WorldTilePosition position,
        RealmId ownerRealmId,
        const SettlementFoundationProfile& foundationProfile
    )
    {
        const Realm* ownerRealm = realms_.find(ownerRealmId);
        if (!canFoundSettlementAt(position, ownerRealmId) || !ownerRealm)
        {
            return {};
        }

        const SettlementId settlementId = settlements_.create(
            position,
            std::string{},
            ownerRealmId,
            ownerRealm->primaryCultureId(),
            foundationProfile
        );

        // A not-yet-established realm keeps the legacy provisional claim until
        // its origin is chosen. Once tribal, settlements never write binary
        // controller cells; their population automatically feeds the field.
        if (!ownerRealm->usesTribalInfluence())
        {
            static_cast<void>(
                TerritoryFoundationSystem{}.establishSettlementTerritory(
                    grid_,
                    territory_,
                    position,
                    ownerRealmId,
                    territoryFoundationPolicy_,
                    territoryFoundationPolicy_.settlementBorderlandTraversalBudget
                )
            );
        }

        return settlementId;
    }


    SettlementId World::foundCapitalSettlement(
        WorldTilePosition position,
        RealmId ownerRealmId,
        const FoundingIdentity& identity
    )
    {
        return foundCapitalSettlement(
            position,
            ownerRealmId,
            identity,
            defaultSettlementFoundationProfile()
        );
    }


    SettlementId World::foundCapitalSettlement(
        WorldTilePosition position,
        RealmId ownerRealmId,
        const FoundingIdentity& identity,
        const SettlementFoundationProfile& foundationProfile
    )
    {
        Realm* ownerRealm = realms_.find(ownerRealmId);

        if (!ownerRealm || ownerRealm->capitalSettlementId().isValid() ||
            !canFoundSettlementAt(position, ownerRealmId) ||
            !isValidFoundingName(identity.realmName) ||
            !isValidFoundingName(identity.cultureName) ||
            !isValidFoundingName(identity.capitalName) ||
            !isKnownRealmOrigin(identity.realmOriginId) ||
            !identity.flag.isValid())
        {
            return {};
        }

        std::string realmName = trimFoundingName(identity.realmName);

        std::string cultureName = trimFoundingName(identity.cultureName);

        std::string capitalName = trimFoundingName(identity.capitalName);

        std::string originId = identity.realmOriginId;

        const CultureId cultureId = cultures_.create(std::move(cultureName));

        SettlementId settlementId;

        try
        {
            settlementId = settlements_.create(
                position,
                std::move(capitalName),
                ownerRealmId,
                cultureId,
                foundationProfile
            );
        }
        catch (...)
        {
            cultures_.erase(cultureId);
            throw;
        }

        ownerRealm->establishCapital(
            settlementId,
            cultureId,
            identity.mapColor,
            std::move(realmName),
            std::move(originId),
            identity.flag
        );

        if (ownerRealm->usesTribalInfluence())
        {
            // Clear any provisional pre-capital cells. From this point forward
            // tribal authority is exclusively the continuous influence field.
            territory_.clearController(ownerRealmId);
        }
        else
        {
            static_cast<void>(
                TerritoryFoundationSystem{}.establishSettlementTerritory(
                    grid_,
                    territory_,
                    position,
                    ownerRealmId,
                    territoryFoundationPolicy_,
                    territoryFoundationPolicy_.capitalBorderlandTraversalBudget
                )
            );
        }

        return settlementId;
    }


    // ========================================================
    // Lookup
    // ========================================================

    Settlement* World::settlement(SettlementId id) noexcept
    {
        return settlements_.find(id);
    }


    const Settlement* World::settlement(SettlementId id) const noexcept
    {
        return settlements_.find(id);
    }


    Realm* World::realm(RealmId id) noexcept
    {
        return realms_.find(id);
    }


    const Realm* World::realm(RealmId id) const noexcept
    {
        return realms_.find(id);
    }


    Culture* World::culture(CultureId id) noexcept
    {
        return cultures_.find(id);
    }


    const Culture* World::culture(CultureId id) const noexcept
    {
        return cultures_.find(id);
    }


    Army* World::army(ArmyId id) noexcept
    {
        return armies_.find(id);
    }


    const Army* World::army(ArmyId id) const noexcept
    {
        return armies_.find(id);
    }


    // ========================================================
    // Settlement relationships
    // ========================================================

    bool World::assignSettlementToRealm(
        SettlementId settlementId,
        RealmId realmId
    ) noexcept
    {
        Settlement* targetSettlement = settlements_.find(settlementId);

        const Realm* targetRealm = realms_.find(realmId);

        if (!targetSettlement || !targetRealm)
        {
            return false;
        }

        targetSettlement->setOwnerRealm(realmId);

        return true;
    }


    bool World::makeSettlementIndependent(SettlementId settlementId) noexcept
    {
        Settlement* targetSettlement = settlements_.find(settlementId);

        if (!targetSettlement)
        {
            return false;
        }

        targetSettlement->setOwnerRealm(RealmId{});

        return true;
    }


    bool World::setSettlementPosition(
        SettlementId settlementId,
        WorldTilePosition position
    ) noexcept
    {
        Settlement* targetSettlement = settlements_.find(settlementId);

        if (!targetSettlement)
        {
            return false;
        }

        targetSettlement->setPosition(position);

        return true;
    }


    bool World::renameSettlement(SettlementId settlementId, std::string name)
    {
        Settlement* targetSettlement = settlements_.find(settlementId);

        if (!targetSettlement || !isValidFoundingName(name))
        {
            return false;
        }

        targetSettlement->setName(trimFoundingName(name));
        return true;
    }


    bool World::editRealmIdentity(
        RealmId realmId,
        const FoundingIdentity& identity
    )
    {
        Realm* targetRealm = realms_.find(realmId);

        if (!targetRealm || !isValidFoundingName(identity.realmName) ||
            !isValidFoundingName(identity.cultureName) ||
            !isKnownRealmOrigin(identity.realmOriginId) ||
            !identity.flag.isValid())
        {
            return false;
        }

        Culture* primaryCulture =
            cultures_.find(targetRealm->primaryCultureId());

        if (!primaryCulture)
        {
            return false;
        }

        const bool wasTribal = targetRealm->usesTribalInfluence();
        const bool becomesTribal = identity.realmOriginId == "tribal";

        primaryCulture->setName(trimFoundingName(identity.cultureName));

        targetRealm->editIdentity(
            identity.mapColor,
            trimFoundingName(identity.realmName),
            identity.realmOriginId,
            identity.flag
        );

        if (!wasTribal && becomesTribal)
        {
            territory_.clearController(realmId);
        }
        else if (wasTribal && !becomesTribal)
        {
            // Converting to civic sovereignty materializes discrete control from
            // all existing settlements. The capital receives the established
            // capital borderland budget; ordinary settlements use theirs.
            for (const Settlement& settlement : settlements_.entities())
            {
                if (settlement.ownerRealmId() != realmId)
                {
                    continue;
                }
                const bool capital =
                    settlement.id() == targetRealm->capitalSettlementId();
                static_cast<void>(
                    TerritoryFoundationSystem{}.establishSettlementTerritory(
                        grid_,
                        territory_,
                        settlement.position(),
                        realmId,
                        territoryFoundationPolicy_,
                        capital
                            ? territoryFoundationPolicy_
                                  .capitalBorderlandTraversalBudget
                            : territoryFoundationPolicy_
                                  .settlementBorderlandTraversalBudget
                    )
                );
            }
        }

        return true;
    }


    bool World::relocateSoleCapital(RealmId realmId, WorldTilePosition position)
    {
        Realm* targetRealm = realms_.find(realmId);

        if (!targetRealm || !canFoundSettlementAt(position, realmId))
        {
            return false;
        }

        Settlement* capital =
            settlements_.find(targetRealm->capitalSettlementId());

        if (!capital)
        {
            return false;
        }

        std::size_t ownedSettlementCount = 0;

        for (const Settlement& settlement : settlements_.entities())
        {
            if (settlement.ownerRealmId() == realmId)
            {
                ++ownedSettlementCount;
            }
        }

        // This setup operation deliberately cannot erase territory belonging
        // to additional settlements. A later colony/capital-transfer system
        // can provide territory provenance for established civic realms.
        if (ownedSettlementCount != 1)
        {
            return false;
        }

        if (targetRealm->usesTribalInfluence())
        {
            // No controller cells exist to relocate. Moving the power center is
            // enough; the derived field notices the new position on next read.
            capital->setPosition(position);
            return true;
        }

        territory_.clearController(realmId);
        capital->setPosition(position);

        static_cast<void>(
            TerritoryFoundationSystem{}.establishSettlementTerritory(
                grid_,
                territory_,
                position,
                realmId,
                territoryFoundationPolicy_,
                territoryFoundationPolicy_.capitalBorderlandTraversalBudget
            )
        );

        return true;
    }


    // ========================================================
    // Army relationships
    // ========================================================

    bool World::assignArmyToRealm(ArmyId armyId, RealmId realmId) noexcept
    {
        Army* targetArmy = armies_.find(armyId);

        const Realm* targetRealm = realms_.find(realmId);

        if (!targetArmy || !targetRealm)
        {
            return false;
        }

        targetArmy->setOwnerRealm(realmId);

        return true;
    }


    bool World::makeArmyIndependent(ArmyId armyId) noexcept
    {
        Army* targetArmy = armies_.find(armyId);

        if (!targetArmy)
        {
            return false;
        }

        targetArmy->setOwnerRealm(RealmId{});

        return true;
    }


    bool World::setArmyPosition(
        ArmyId armyId,
        WorldTilePosition position
    ) noexcept
    {
        Army* targetArmy = armies_.find(armyId);

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


    std::size_t World::realmCount() const noexcept
    {
        return realms_.size();
    }


    std::size_t World::cultureCount() const noexcept
    {
        return cultures_.size();
    }


    std::size_t World::armyCount() const noexcept
    {
        return armies_.size();
    }
} // namespace Paladin
