#include "TestFramework.h"

#include "simulation/WorldSimulationPipeline.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementInspectionController.h"
#include "world/TerrainType.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <array>
#include <queue>
#include <utility>
#include <vector>

void runWorldTests()
{
    Paladin::WorldGenerationSettings settings;
    settings.width = 96;
    settings.height = 72;

    Paladin::World world(settings);

    const Paladin::PolityId polityId =
        world.createPolity();

    Paladin::WorldTilePosition foundingPosition{};
    bool foundLandTile = false;

    for (
        std::int32_t y = 0;
        y < world.grid().height() && !foundLandTile;
        ++y
    )
    {
        for (
            std::int32_t x = 0;
            x < world.grid().width();
            ++x
        )
        {
            const Paladin::WorldTile* tile =
                world.grid().tile({x, y});

            if (
                tile->terrain == Paladin::TerrainType::Land &&
                world.canFoundSettlementAt({x, y})
            )
            {
                foundingPosition = {x, y};
                foundLandTile = true;
                break;
            }
        }
    }

    PALADIN_CHECK(foundLandTile);

    const Paladin::SettlementId settlementId =
        world.foundSettlement(
            foundingPosition,
            polityId
        );

    PALADIN_CHECK(settlementId.isValid());

    PALADIN_CHECK(
        world.territory().controllerAt({
            foundingPosition.x,
            foundingPosition.y
        }) == polityId
    );

    PALADIN_CHECK(
        !world.foundSettlement(
            foundingPosition,
            polityId
        ).isValid()
    );

    Paladin::Settlement* settlement =
        world.settlement(settlementId);

    PALADIN_CHECK(settlement != nullptr);

    PALADIN_CHECK(
        settlement->hasOwnerPolity()
    );

    PALADIN_CHECK(
        settlement->ownerPolityId()
        == polityId
    );

    PALADIN_CHECK(
        settlement->simulationState().isInitialized()
    );

    PALADIN_CHECK(
        settlement->simulationState().population().residents()
        == 100
    );

    PALADIN_CHECK(
        settlement->simulationState().stockpile().amount("food")
        == 600.0
    );

    PALADIN_CHECK(
        settlement->simulationState().stockpile().amount("materials")
        == 120.0
    );

    PALADIN_CHECK(
        world.makeSettlementIndependent(
            settlementId
        )
    );

    PALADIN_CHECK(
        !settlement->hasOwnerPolity()
    );

    PALADIN_CHECK(
        world.assignSettlementToPolity(
            settlementId,
            polityId
        )
    );

    PALADIN_CHECK(
        settlement->ownerPolityId() == polityId
    );

    Paladin::WorldTilePosition capitalPosition{
        foundingPosition.x + 1,
        foundingPosition.y
    };

    if (!world.canFoundSettlementAt(capitalPosition))
    {
        for (
            std::int32_t y = 0;
            y < world.grid().height();
            ++y
        )
        {
            for (
                std::int32_t x = 0;
                x < world.grid().width();
                ++x
            )
            {
                if (world.canFoundSettlementAt({x, y}))
                {
                    capitalPosition = {x, y};
                    y = world.grid().height();
                    break;
                }
            }
        }
    }

    const Paladin::SettlementId capitalId =
        world.foundCapitalSettlement(
            capitalPosition,
            polityId,
            {
                "  Dawn Dominion  ",
                "  Dawnfolk  ",
                "  New Dawn  ",
                {42, 86, 190},
                "civic"
            }
        );

    PALADIN_CHECK(capitalId.isValid());

    PALADIN_CHECK(
        world.territory().controllerAt({
            capitalPosition.x,
            capitalPosition.y
        }) == polityId
    );

    const Paladin::Polity* polity = world.polity(polityId);
    const Paladin::Settlement* capital = world.settlement(capitalId);

    PALADIN_CHECK(polity != nullptr);
    PALADIN_CHECK(capital != nullptr);

    const Paladin::Culture* culture =
        world.culture(polity->primaryCultureId());

    PALADIN_CHECK(culture != nullptr);
    const Paladin::MapColor expectedMapColor{42, 86, 190};
    PALADIN_CHECK(polity->capitalSettlementId() == capitalId);
    PALADIN_CHECK(polity->mapColor() == expectedMapColor);
    PALADIN_CHECK(polity->name() == "Dawn Dominion");
    PALADIN_CHECK(polity->startingOriginId() == "civic");
    PALADIN_CHECK(capital->name() == "New Dawn");
    PALADIN_CHECK(capital->primaryCultureId() == culture->id());
    PALADIN_CHECK(culture->name() == "Dawnfolk");
    PALADIN_CHECK(polity->flag().width == 7);
    PALADIN_CHECK(polity->flag().height == 9);
    PALADIN_CHECK(polity->flag().cells.size() == 63);

    Paladin::FoundingIdentity editedIdentity{
        "Dawn Realm",
        "Dawnkin",
        "",
        {170, 62, 96},
        "tribal"
    };
    editedIdentity.flag.primaryColor = {32, 180, 110};
    editedIdentity.flag.cells[0] = {
        true,
        editedIdentity.flag.primaryColor
    };

    PALADIN_CHECK(
        world.editPolityIdentity(polityId, editedIdentity)
    );
    PALADIN_CHECK(
        world.renameSettlement(capitalId, "First Light")
    );

    polity = world.polity(polityId);
    capital = world.settlement(capitalId);
    culture = world.culture(polity->primaryCultureId());

    PALADIN_CHECK(polity->name() == "Dawn Realm");
    PALADIN_CHECK(culture->name() == "Dawnkin");
    PALADIN_CHECK(capital->name() == "First Light");
    PALADIN_CHECK(polity->startingOriginId() == "tribal");
    PALADIN_CHECK(polity->mapColor() == editedIdentity.mapColor);
    PALADIN_CHECK(polity->flag() == editedIdentity.flag);

    Paladin::WorldTilePosition aiCapitalPosition{};
    bool foundAiCapitalPosition = false;

    for (
        std::int32_t y = 0;
        y < world.grid().height() && !foundAiCapitalPosition;
        ++y
    )
    {
        for (
            std::int32_t x = 0;
            x < world.grid().width();
            ++x
        )
        {
            if (world.canFoundSettlementAt({x, y}))
            {
                aiCapitalPosition = {x, y};
                foundAiCapitalPosition = true;
                break;
            }
        }
    }

    PALADIN_CHECK(foundAiCapitalPosition);

    const Paladin::PolityId aiPolityId =
        world.createPolity();

    Paladin::SettlementFoundationProfile aiFoundationProfile =
        Paladin::defaultSettlementFoundationProfile();

    bool hasStone = false;
    bool hasLumber = false;
    for (const Paladin::StockpileEntry& resource :
        aiFoundationProfile.initialResources)
    {
        hasStone = hasStone ||
            resource.resourceId == Paladin::SettlementResourceTypes::Stone;
        hasLumber = hasLumber ||
            resource.resourceId == Paladin::SettlementResourceTypes::Lumber;
    }
    PALADIN_CHECK(hasStone);
    PALADIN_CHECK(hasLumber);

    aiFoundationProfile.initialSimulationTier =
        Paladin::SettlementSimulationTier::Strategic;

    aiFoundationProfile.demographicRates =
        {0.0, 0.0, 0.0, 0.20, 0.0};

    for (Paladin::StockpileEntry& resource
        : aiFoundationProfile.initialResources)
    {
        if (resource.resourceId == "food")
        {
            resource.amount = 0.0;
        }
    }

    for (Paladin::ResourceFlowRate& flowRate
        : aiFoundationProfile.resourceFlowRates)
    {
        if (flowRate.resourceId == "food")
        {
            flowRate.dailyProductionPerResident = 0.0;
        }
    }

    const Paladin::SettlementId aiCapitalId =
        world.foundCapitalSettlement(
            aiCapitalPosition,
            aiPolityId,
            {
                "River Confederacy",
                "Riverfolk",
                "Riverhold",
                {55, 145, 95},
                "tribal"
            },
            aiFoundationProfile
        );

    PALADIN_CHECK(aiCapitalId.isValid());

    PALADIN_CHECK(
        world.territory().controllerAt({
            aiCapitalPosition.x,
            aiCapitalPosition.y
        }) == aiPolityId
    );

    for (std::int32_t y = 0; y < world.grid().height(); ++y)
    {
        for (std::int32_t x = 0; x < world.grid().width(); ++x)
        {
            const Paladin::WorldTile* tile =
                world.grid().tile({x, y});

            if (tile->terrain == Paladin::TerrainType::Water)
            {
                PALADIN_CHECK(
                    !world.territory().isControlled({x, y})
                );
            }
        }
    }

    Paladin::Settlement* simulatedPlayerCapital =
        world.settlement(capitalId);

    Paladin::Settlement* simulatedAiCapital =
        world.settlement(aiCapitalId);

    Paladin::Settlement* simulatedInactiveSettlement =
        world.settlement(settlementId);

    PALADIN_CHECK(simulatedPlayerCapital != nullptr);
    PALADIN_CHECK(simulatedAiCapital != nullptr);
    PALADIN_CHECK(simulatedInactiveSettlement != nullptr);

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .population().residents() == 100
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .population().residents() == 100
    );

    Paladin::WorldSimulationPipeline simulationPipeline;

    PALADIN_CHECK(simulationPipeline.systemCount() == 2);
    PALADIN_CHECK(
        simulationPipeline.transitionSettlementTier(
            world,
            capitalId,
            Paladin::SettlementSimulationTier::Detailed
        )
    );
    PALADIN_CHECK(
        simulationPipeline.policies().detailed.minimumStepMinutes == 1
    );
    PALADIN_CHECK(
        simulationPipeline.policies().inactive.minimumStepMinutes == 60
    );

    const double playerOpeningFood =
        simulatedPlayerCapital->simulationState()
            .stockpile().amount("food");

    const double aiOpeningFood =
        simulatedAiCapital->simulationState()
            .stockpile().amount("food");

    const double inactiveOpeningFood =
        simulatedInactiveSettlement->simulationState()
            .stockpile().amount("food");

    const Paladin::SettlementStateVersions playerOpeningVersions =
        simulatedPlayerCapital->simulationState().versions();

    const Paladin::SettlementStateVersions inactiveOpeningVersions =
        simulatedInactiveSettlement->simulationState().versions();

    constexpr std::uint64_t gameMinutesPerDay =
        24 * 60;

    simulationPipeline.tick(
        world,
        30
    );

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .stockpile().amount("food") > playerOpeningFood
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .stockpile().amount("food") == aiOpeningFood
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .stockpile().amount("food") == inactiveOpeningFood
    );

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .totalSimulatedMinutes() == 30
    );

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .versions().resources > playerOpeningVersions.resources
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .pendingSimulationMinutes() == 30
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .versions().resources == inactiveOpeningVersions.resources
    );

    const Paladin::SettlementStateChanges playerChanges =
        simulatedPlayerCapital->simulationState().changesSince(
            playerOpeningVersions
        );

    PALADIN_CHECK(
        playerChanges.has(Paladin::SettlementStateDomain::Resources)
    );

    PALADIN_CHECK(
        playerChanges.has(Paladin::SettlementStateDomain::Economy)
    );

    const Paladin::SettlementStateChanges inactivePendingChanges =
        simulatedInactiveSettlement->simulationState().changesSince(
            inactiveOpeningVersions
        );

    PALADIN_CHECK(
        inactivePendingChanges.has(
            Paladin::SettlementStateDomain::Scheduling
        )
    );

    PALADIN_CHECK(
        !inactivePendingChanges.has(
            Paladin::SettlementStateDomain::Resources
        )
    );

    PALADIN_CHECK(
        simulationPipeline.transitionSettlementTier(
            world,
            settlementId,
            Paladin::SettlementSimulationTier::Detailed
        )
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .pendingSimulationMinutes() == 0
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .totalSimulatedMinutes() == 30
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .stockpile().amount("food") > inactiveOpeningFood
    );

    const double inactiveFoodAfterTransition =
        simulatedInactiveSettlement->simulationState()
            .stockpile().amount("food");

    PALADIN_CHECK(
        simulationPipeline.transitionSettlementTier(
            world,
            settlementId,
            Paladin::SettlementSimulationTier::Inactive
        )
    );

    simulationPipeline.tick(
        world,
        60
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .stockpile().amount("food") > inactiveFoodAfterTransition
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .pendingSimulationMinutes() == 0
    );

    PALADIN_CHECK(
        simulatedInactiveSettlement->simulationState()
            .totalSimulatedMinutes() == 90
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .population().residents() == 100
    );

    simulationPipeline.tick(
        world,
        30 * gameMinutesPerDay - 90
    );

    const std::uint64_t aiPopulationAfterStrategicStep =
        simulatedAiCapital->simulationState()
            .population().residents();

    PALADIN_CHECK(
        aiPopulationAfterStrategicStep < 100
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .pendingSimulationMinutes() == 0
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .totalSimulatedMinutes() == 30 * gameMinutesPerDay
    );

    constexpr std::uint64_t gameMinutesPerYear =
        365 * gameMinutesPerDay;

    simulationPipeline.tick(
        world,
        gameMinutesPerYear
    );

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .population().residents() > 100
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .population().residents() <
                aiPopulationAfterStrategicStep
    );

    Paladin::WorldGenerationSettings borderlandSettings;
    borderlandSettings.width = 32;
    borderlandSettings.height = 32;

    Paladin::TerritoryFoundationPolicy borderlandPolicy =
        Paladin::defaultTerritoryFoundationPolicy();

    Paladin::World borderlandWorld(
        borderlandSettings,
        borderlandPolicy
    );

    for (
        std::int32_t y = 0;
        y < borderlandWorld.grid().height();
        ++y
    )
    {
        for (
            std::int32_t x = 0;
            x < borderlandWorld.grid().width();
            ++x
        )
        {
            borderlandWorld.grid().tile({x, y})->terrain =
                Paladin::TerrainType::Land;
        }
    }

    const Paladin::PolityId borderlandPolityId =
        borderlandWorld.createPolity();

    constexpr Paladin::WorldTilePosition borderlandCapital{16, 16};

    PALADIN_CHECK(
        borderlandWorld.foundCapitalSettlement(
            borderlandCapital,
            borderlandPolityId,
            {
                "Borderland Test Polity",
                "Borderland Test Culture",
                "Borderland Test Capital",
                {120, 80, 190},
                "civic"
            }
        ).isValid()
    );

    const std::int32_t regionHalfWidth =
        borderlandPolicy.settlementRegionWidth / 2;

    const std::int32_t regionHalfHeight =
        borderlandPolicy.settlementRegionHeight / 2;

    const std::int32_t regionMinimumX =
        borderlandCapital.x - regionHalfWidth;

    const std::int32_t regionMaximumX =
        regionMinimumX
        + borderlandPolicy.settlementRegionWidth - 1;

    const std::int32_t regionMinimumY =
        borderlandCapital.y - regionHalfHeight;

    const std::int32_t regionMaximumY =
        regionMinimumY
        + borderlandPolicy.settlementRegionHeight - 1;

    std::size_t claimedImmediateBorderlandTiles = 0;
    std::size_t possibleImmediateBorderlandTiles = 0;

    const auto countImmediateBorderlandTile =
        [&](Paladin::WorldTilePosition position)
        {
            ++possibleImmediateBorderlandTiles;

            if (
                borderlandWorld.territory().controllerAt(position)
                == borderlandPolityId
            )
            {
                ++claimedImmediateBorderlandTiles;
            }
        };

    for (
        std::int32_t x = regionMinimumX;
        x <= regionMaximumX;
        ++x
    )
    {
        countImmediateBorderlandTile({x, regionMinimumY - 1});
        countImmediateBorderlandTile({x, regionMaximumY + 1});
    }

    for (
        std::int32_t y = regionMinimumY;
        y <= regionMaximumY;
        ++y
    )
    {
        countImmediateBorderlandTile({regionMinimumX - 1, y});
        countImmediateBorderlandTile({regionMaximumX + 1, y});
    }

    PALADIN_CHECK(claimedImmediateBorderlandTiles > 0);
    PALADIN_CHECK(
        claimedImmediateBorderlandTiles
        < possibleImmediateBorderlandTiles
    );

    const std::array<Paladin::WorldTilePosition, 4>
        cardinalNeighborOffsets{
            Paladin::WorldTilePosition{-1, 0},
            Paladin::WorldTilePosition{1, 0},
            Paladin::WorldTilePosition{0, -1},
            Paladin::WorldTilePosition{0, 1}
        };

    std::vector<bool> visited(
        borderlandWorld.grid().tileCount(),
        false
    );

    std::queue<Paladin::WorldTilePosition> frontier;
    frontier.push({borderlandCapital.x, borderlandCapital.y});

    std::size_t connectedTileCount = 0;

    while (!frontier.empty())
    {
        const Paladin::WorldTilePosition position = frontier.front();
        frontier.pop();

        const std::size_t index =
            static_cast<std::size_t>(position.y)
                * static_cast<std::size_t>(
                    borderlandWorld.grid().width()
                )
            + static_cast<std::size_t>(position.x);

        if (visited[index])
        {
            continue;
        }

        visited[index] = true;
        ++connectedTileCount;

        for (const Paladin::WorldTilePosition offset
            : cardinalNeighborOffsets)
        {
            const Paladin::WorldTilePosition neighbor{
                position.x + offset.x,
                position.y + offset.y
            };

            if (
                borderlandWorld.territory().controllerAt(neighbor)
                == borderlandPolityId
            )
            {
                frontier.push(neighbor);
            }
        }
    }

    PALADIN_CHECK(
        connectedTileCount
        == borderlandWorld.territory().controlledTileCount(
            borderlandPolityId
        )
    );

    constexpr Paladin::WorldTilePosition movedCapital{7, 7};
    PALADIN_CHECK(
        borderlandWorld.relocateSoleCapital(
            borderlandPolityId,
            movedCapital
        )
    );
    PALADIN_CHECK(
        borderlandWorld.settlement(
            borderlandWorld.polity(borderlandPolityId)
                ->capitalSettlementId()
        )->position() == movedCapital
    );
    PALADIN_CHECK(
        borderlandWorld.territory().controllerAt({7, 7})
        == borderlandPolityId
    );
    PALADIN_CHECK(
        !borderlandWorld.territory().controllerAt({16, 16}).isValid()
    );

    Paladin::World freshMovedWorld(
        borderlandSettings,
        borderlandPolicy
    );

    for (std::int32_t y = 0; y < freshMovedWorld.grid().height(); ++y)
    {
        for (std::int32_t x = 0; x < freshMovedWorld.grid().width(); ++x)
        {
            freshMovedWorld.grid().tile({x, y})->terrain =
                Paladin::TerrainType::Land;
        }
    }

    const Paladin::PolityId freshMovedPolityId =
        freshMovedWorld.createPolity();

    PALADIN_CHECK(
        freshMovedWorld.foundCapitalSettlement(
            movedCapital,
            freshMovedPolityId,
            {
                "Fresh Move Test Polity",
                "Fresh Move Test Culture",
                "Fresh Move Test Capital",
                {120, 80, 190},
                "civic"
            }
        ).isValid()
    );

    for (std::int32_t y = 0; y < borderlandWorld.grid().height(); ++y)
    {
        for (std::int32_t x = 0; x < borderlandWorld.grid().width(); ++x)
        {
            PALADIN_CHECK(
                borderlandWorld.territory()
                    .controllerAt({x, y}).isValid()
                == freshMovedWorld.territory()
                    .controllerAt({x, y}).isValid()
            );
        }
    }

    Paladin::SettlementGrid localGrid(24, 24);

    for (std::int32_t y = 0; y < localGrid.height(); ++y)
    {
        for (std::int32_t x = 0; x < localGrid.width(); ++x)
        {
            localGrid.tile({x, y})->terrain =
                Paladin::TerrainType::Land;
        }
    }

    localGrid.tile({15, 15})->terrain =
        Paladin::TerrainType::Mountain;

    Paladin::SettlementMap localMap(
        std::move(localGrid),
        {12, 12},
        9,
        9,
        64,
        44
    );

    Paladin::SettlementObjectPlacementController objectPlacement;

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::FishingGrounds
    ));

    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{2, 2});
    PALADIN_CHECK(!objectPlacement.visibleFootprintIsValid(localMap));

    PALADIN_CHECK(
        objectPlacement.pointerPressed({{2, 2}}, localMap)
        == Paladin::SettlementPlacementCommitResult::None
    );
    PALADIN_CHECK(!objectPlacement.pointerReleased({{2, 2}}, localMap));
    PALADIN_CHECK(!objectPlacement.hasLockedFootprint());

    PALADIN_CHECK(
        objectPlacement.pointerPressed({{2, 2}}, localMap)
        == Paladin::SettlementPlacementCommitResult::None
    );
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{3, 3});
    PALADIN_CHECK(objectPlacement.pointerReleased({{3, 3}}, localMap));
    PALADIN_CHECK(objectPlacement.hasLockedFootprint());

    PALADIN_CHECK(
        objectPlacement.pointerPressed({{23, 23}}, localMap)
        == Paladin::SettlementPlacementCommitResult::ConstructionSites
    );
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 1);
    PALADIN_CHECK(
        localMap.objectState().constructionSites().front().phase ==
            Paladin::ConstructionSitePhase::AwaitingMaterials
    );
    PALADIN_CHECK(localMap.objectState().completedObjects().empty());

    const Paladin::SettlementObjectDefinition* stockpileDefinition =
        Paladin::SettlementObjectCatalog::definition(
            Paladin::SettlementObjectTypes::Stockpile
        );

    PALADIN_CHECK(stockpileDefinition != nullptr);
    Paladin::SettlementGrid unrestrictedSizeGrid(30, 2);
    for (std::int32_t x = 0; x < unrestrictedSizeGrid.width(); ++x)
    {
        unrestrictedSizeGrid.tile({x, 0})->terrain =
            Paladin::TerrainType::Land;
        unrestrictedSizeGrid.tile({x, 1})->terrain =
            Paladin::TerrainType::Land;
    }
    const Paladin::SettlementObjectState unrestrictedSizeState(30, 2);
    PALADIN_CHECK(
        unrestrictedSizeState.canPlace(
            unrestrictedSizeGrid,
            *stockpileDefinition,
            {{0, 0}, 30, 2}
        )
    );

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::House
    ));
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{8, 8});
    PALADIN_CHECK(objectPlacement.visibleFootprintIsValid(localMap));
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{8, 8}}, localMap)
        == Paladin::SettlementPlacementCommitResult::ConstructionSites
    );
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 2);

    const Paladin::SettlementConstructionSite* houseSite =
        localMap.objectState().constructionSiteAt({8, 8});
    PALADIN_CHECK(houseSite != nullptr);
    PALADIN_CHECK(houseSite->progressPermille == 0);
    PALADIN_CHECK(houseSite->resourceDeliveries.size() == 2);
    PALADIN_CHECK(
        houseSite->resourceDeliveries[0].resourceId ==
            Paladin::SettlementResourceTypes::Lumber
    );
    PALADIN_CHECK(houseSite->resourceDeliveries[0].requiredAmount == 0);
    PALADIN_CHECK(
        houseSite->resourceDeliveries[1].resourceId ==
            Paladin::SettlementResourceTypes::Stone
    );

    Paladin::SettlementCitizenState citizens;
    Paladin::SettlementInspectionController inspection;
    PALADIN_CHECK(inspection.selectAt(
        {8, 8},
        localMap.objectState(),
        citizens,
        true
    ));
    PALADIN_CHECK(
        inspection.selectedConstructionSite(localMap.objectState()) ==
            houseSite
    );

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::Stockpile
    ));
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{15, 15}}, localMap)
        == Paladin::SettlementPlacementCommitResult::None
    );
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{16, 16});
    PALADIN_CHECK(!objectPlacement.pointerReleased({{16, 16}}, localMap));
    PALADIN_CHECK(!objectPlacement.hasLockedFootprint());
    objectPlacement.cancelPlacement();
    PALADIN_CHECK(!objectPlacement.isActive());

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::Road
    ));
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{10, 15}}, localMap)
        == Paladin::SettlementPlacementCommitResult::None
    );
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{14, 15});
    PALADIN_CHECK(objectPlacement.pointerReleased({{14, 15}}, localMap));
    PALADIN_CHECK(
        objectPlacement.pointerPressed(std::nullopt, localMap)
        == Paladin::SettlementPlacementCommitResult::ConstructionSites
    );
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 3);

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::Road
    ));
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{12, 15}}, localMap)
        == Paladin::SettlementPlacementCommitResult::None
    );
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{16, 15});
    PALADIN_CHECK(objectPlacement.pointerReleased({{16, 15}}, localMap));

    const Paladin::SettlementObjectDefinition* roadDefinition =
        Paladin::SettlementObjectCatalog::definition(
            Paladin::SettlementObjectTypes::Road
        );

    PALADIN_CHECK(roadDefinition != nullptr);
    PALADIN_CHECK(
        localMap.objectState().placementStatusAt(
            localMap.grid(),
            *roadDefinition,
            {12, 15}
        ) == Paladin::SettlementTilePlacementStatus::Occupied
    );
    PALADIN_CHECK(
        localMap.objectState().placementStatusAt(
            localMap.grid(),
            *roadDefinition,
            {16, 15}
        ) == Paladin::SettlementTilePlacementStatus::Buildable
    );
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{0, 0}}, localMap)
        == Paladin::SettlementPlacementCommitResult::ConstructionSites
    );
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 4);

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::House
    ));
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{12, 15});
    PALADIN_CHECK(objectPlacement.visibleFootprintIsValid(localMap));
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{12, 15}}, localMap)
        == Paladin::SettlementPlacementCommitResult::ConstructionSites
    );
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 6);

    PALADIN_CHECK(objectPlacement.beginPlacement(
        Paladin::SettlementObjectTypes::CityKeep
    ));
    objectPlacement.pointerMoved(Paladin::SettlementTilePosition{19, 10});
    PALADIN_CHECK(
        objectPlacement.pointerPressed({{19, 10}}, localMap)
        == Paladin::SettlementPlacementCommitResult::CompletedObject
    );
    PALADIN_CHECK(localMap.objectState().completedObjects().size() == 1);
    PALADIN_CHECK(localMap.objectState().constructionSites().size() == 6);
    PALADIN_CHECK(inspection.selectAt(
        {19, 10},
        localMap.objectState(),
        citizens,
        false
    ));
    PALADIN_CHECK(
        inspection.selectedObject(localMap.objectState()) != nullptr
    );

    PALADIN_CHECK(citizens.initialize(8, 42));
    citizens.placeUnpositionedCitizens(localMap);
    PALADIN_CHECK(citizens.citizens().size() == 8);
    PALADIN_CHECK(!citizens.citizens().front().name.empty());
    const Paladin::SettlementObjectFootprint keepFootprint =
        localMap.objectState().completedObjects().front().footprint;
    for (const Paladin::SettlementCitizen& citizen : citizens.citizens())
    {
        PALADIN_CHECK(!keepFootprint.contains(citizen.tilePosition));

        const std::int32_t horizontalDistance =
            citizen.tilePosition.x < keepFootprint.topLeft.x
                ? keepFootprint.topLeft.x - citizen.tilePosition.x
                : citizen.tilePosition.x >=
                    keepFootprint.topLeft.x + keepFootprint.width
                    ? citizen.tilePosition.x -
                        (keepFootprint.topLeft.x + keepFootprint.width - 1)
                    : 0;
        const std::int32_t verticalDistance =
            citizen.tilePosition.y < keepFootprint.topLeft.y
                ? keepFootprint.topLeft.y - citizen.tilePosition.y
                : citizen.tilePosition.y >=
                    keepFootprint.topLeft.y + keepFootprint.height
                    ? citizen.tilePosition.y -
                        (keepFootprint.topLeft.y + keepFootprint.height - 1)
                    : 0;

        PALADIN_CHECK(horizontalDistance + verticalDistance == 1);
    }
    PALADIN_CHECK(inspection.selectAt(
        citizens.citizens().front().tilePosition,
        localMap.objectState(),
        citizens,
        true
    ));
    PALADIN_CHECK(
        inspection.selectedCitizen(citizens) ==
            &citizens.citizens().front()
    );
    localMap.naturalFeatures().set({0, 0}, Paladin::NaturalFeatureKind::Tree);
    PALADIN_CHECK(
        localMap.commandState().add(
            localMap,
            Paladin::SettlementCommandTypes::ChopTree,
            {{0, 0}, 24, 24},
            citizens
        )
    );
    PALADIN_CHECK(localMap.commandState().commands().size() == 1);
    PALADIN_CHECK(
        citizens.citizens().front().activity ==
            Paladin::CitizenActivity::Idle
    );
    PALADIN_CHECK(
        localMap.commandState().cancelIntersecting(
            localMap,
            {{0, 0}, 1, 1},
            citizens
        ) == 1
    );
    PALADIN_CHECK(
        citizens.citizens().front().activity ==
            Paladin::CitizenActivity::Idle
    );
}
