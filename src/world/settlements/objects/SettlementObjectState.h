#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    class WorldGrid;
    struct SettlementObjectDefinition;

    struct SettlementObjectFootprint
    {
        WorldTilePosition topLeft;
        std::int32_t width = 0;
        std::int32_t height = 0;

        [[nodiscard]]
        bool contains(WorldTilePosition position) const noexcept;

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
    };

    enum class ConstructionSitePhase : std::uint8_t
    {
        AwaitingConstruction
    };

    struct SettlementConstructionSite
    {
        ConstructionSiteId id;
        std::string objectTypeId;
        SettlementObjectFootprint footprint;
        ConstructionSitePhase phase =
            ConstructionSitePhase::AwaitingConstruction;
    };

    enum class SettlementTilePlacementStatus : std::uint8_t
    {
        Buildable,
        InvalidTerrain,
        Occupied
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
            const WorldGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        ) const noexcept;

        [[nodiscard]]
        SettlementTilePlacementStatus placementStatusAt(
            const WorldGrid& grid,
            const SettlementObjectDefinition& definition,
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        bool placeCompletedObject(
            const WorldGrid& grid,
            const SettlementObjectDefinition& definition,
            const SettlementObjectFootprint& footprint
        );

        [[nodiscard]]
        bool createConstructionSites(
            const WorldGrid& grid,
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
        std::uint64_t presentationVersion() const noexcept;

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
        std::size_t tileIndex(WorldTilePosition position) const noexcept;

        std::int32_t mapWidth_ = 0;
        std::int32_t mapHeight_ = 0;
        std::vector<std::uint8_t> structureOccupiedTiles_;
        std::vector<std::uint8_t> infrastructureOccupiedTiles_;
        std::vector<CompletedSettlementObject> completedObjects_;
        std::vector<SettlementConstructionSite> constructionSites_;
        IdGenerator<SettlementObjectId> objectIds_;
        IdGenerator<ConstructionSiteId> constructionSiteIds_;
        std::uint64_t presentationVersion_ = 0;
    };
}
