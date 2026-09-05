#pragma once

#include "core/StrongId.h"
#include "world/SettlementGrid.h"
#include "world/SettlementTilePosition.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    struct SettlementObjectDefinition;

    struct SettlementObjectFootprint
    {
        SettlementTilePosition topLeft;
        std::int32_t width = 0;
        std::int32_t height = 0;

        [[nodiscard]]
        bool contains(SettlementTilePosition position) const noexcept;

        friend constexpr bool operator==(
            const SettlementObjectFootprint&,
            const SettlementObjectFootprint&
        ) noexcept = default;
    };

    struct CompletedSettlementObject
    {
        SettlementObjectId id;
        std::string objectTypeId;
        SettlementObjectFootprint footprint;
        std::vector<SettlementTilePosition> productionWater;
        double productionProgress = 0;
    };

    enum class ConstructionSitePhase : std::uint8_t
    {
        AwaitingMaterials,
        ReadyToBuild,
        UnderConstruction
    };

    struct ConstructionResourceDelivery
    {
        std::string resourceId;
        std::uint32_t deliveredAmount = 0;
        std::uint32_t requiredAmount = 0;
    };

    struct SettlementConstructionSite
    {
        ConstructionSiteId id;
        std::string objectTypeId;
        SettlementObjectFootprint footprint;
        ConstructionSitePhase phase =
            ConstructionSitePhase::AwaitingMaterials;
        std::uint16_t progressPermille = 0;
        std::vector<ConstructionResourceDelivery> resourceDeliveries;
        std::vector<SettlementTilePosition> productionWater;
        double laborMinutes = 0;
    };

    enum class SettlementTilePlacementStatus : std::uint8_t
    {
        Buildable,
        InvalidTerrain,
        Occupied
    };

    struct SettlementPlacementAreaEvaluation
    {
        std::size_t buildableTileCount = 0;
        std::size_t blockedTileCount = 0;
        bool footprintAllowed = false;

        [[nodiscard]]
        bool hasObstructions() const noexcept
        {
            return blockedTileCount > 0;
        }
    };

    class SettlementObjectState
    {
    public:
        SettlementObjectState(
            std::int32_t mapWidth,
            std::int32_t mapHeight
        );

        [[nodiscard]]
        bool canPlace(
            const SettlementGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        ) const noexcept;

        [[nodiscard]]
        SettlementPlacementAreaEvaluation evaluatePlacementArea(
            const SettlementGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        ) const noexcept;

        [[nodiscard]]
        SettlementTilePlacementStatus placementStatusAt(
            const SettlementGrid& grid,
            const SettlementObjectDefinition& definition,
            SettlementTilePosition position
        ) const noexcept;

        [[nodiscard]]
        bool placeCompletedObject(
            const SettlementGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        );

        [[nodiscard]]
        bool createConstructionSites(
            const SettlementGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        );

        [[nodiscard]]
        const std::vector<CompletedSettlementObject>&
        completedObjects() const noexcept;

        [[nodiscard]]
        const std::vector<SettlementConstructionSite>&
        constructionSites() const noexcept;

        [[nodiscard]]
        const CompletedSettlementObject* completedObject(
            SettlementObjectId id
        ) const noexcept;

        [[nodiscard]]
        const SettlementConstructionSite* constructionSite(
            ConstructionSiteId id
        ) const noexcept;

        [[nodiscard]]
        const CompletedSettlementObject* completedObjectAt(
            SettlementTilePosition position
        ) const noexcept;

        [[nodiscard]]
        const SettlementConstructionSite* constructionSiteAt(
            SettlementTilePosition position
        ) const noexcept;

        [[nodiscard]]
        std::uint64_t presentationVersion() const noexcept;

        std::uint64_t navigationVersion() const
        {
            return navigationVersion_;
        }
        bool hasCityKeep() const noexcept;
        bool deliverMaterials(
            ConstructionSiteId,
            std::string_view resource,
            int amount
        );
        SettlementObjectId build(
            ConstructionSiteId,
            double laborMinutes,
            double requiredMinutes,
            SettlementTilePosition workerTile
        );
        bool demolish(SettlementObjectId, SettlementTilePosition tile);
        double accrueProduction(SettlementObjectId, double amount);
        void rebuildOccupancy();
        std::size_t cancelConstructionWithin(const SettlementObjectFootprint& area);
        bool blocksMovement(SettlementTilePosition position) const noexcept
        {
            return position.x < 0 || position.y < 0 ||
                   position.x >= mapWidth_ || position.y >= mapHeight_ ||
                   movementBlockedTiles_[tileIndex(position)] != 0;
        }

    private:
        [[nodiscard]]
        bool footprintSizeIsAllowed(
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        ) const noexcept;

        [[nodiscard]]
        bool hasObjectType(std::string_view objectTypeId) const noexcept;

        void removeInfrastructureWithin(
            const SettlementObjectFootprint& footprint
        );

        void occupy(
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        ) noexcept;

        void clearInfrastructureOccupancy(
            const SettlementObjectFootprint& footprint
        ) noexcept;

        [[nodiscard]]
        std::size_t tileIndex(SettlementTilePosition position) const noexcept;

        void rebuildPlacementPrefixCache(
            const SettlementGrid& grid
        ) const noexcept;

        [[nodiscard]]
        std::uint32_t blockedTileCountIn(
            const std::vector<std::uint32_t>& prefix,
            const SettlementObjectFootprint& footprint
        ) const noexcept;

        std::int32_t mapWidth_ = 0;
        std::int32_t mapHeight_ = 0;
        std::vector<std::uint8_t> structureOccupiedTiles_;
        std::vector<std::uint8_t> movementBlockedTiles_;
        std::vector<std::uint8_t> infrastructureOccupiedTiles_;
        std::vector<CompletedSettlementObject> completedObjects_;
        std::vector<SettlementConstructionSite> constructionSites_;
        IdGenerator<SettlementObjectId> objectIds_;
        IdGenerator<ConstructionSiteId> constructionSiteIds_;
        std::uint64_t presentationVersion_ = 0;
        std::uint64_t navigationVersion_ = 0;
        mutable const SettlementGrid* cachedPlacementGrid_ = nullptr;
        mutable std::uint64_t cachedPlacementVersion_ =
            static_cast<std::uint64_t>(-1);
        mutable std::vector<std::uint32_t> structureBlockedPrefix_;
        mutable std::vector<std::uint32_t> infrastructureBlockedPrefix_;
    };
}
