#pragma once

#include "core/StrongId.h"
#include "world/SettlementGrid.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    class SettlementCitizenState;
    class SettlementMap;

    struct SettlementCommandTarget
    {
        SettlementObjectFootprint footprint;
        SettlementObjectId objectId;
        ConstructionSiteId constructionId;
    };

    struct SettlementCommand
    {
        SettlementCommandId id;
        std::string commandTypeId;
        std::vector<SettlementCommandTarget> targets;
        std::unordered_map<std::uint64_t, std::size_t> targetIndex;
    };

    class SettlementCommandState
    {
    public:
        [[nodiscard]]
        bool add(
            SettlementMap& map,
            std::string_view commandTypeId,
            const SettlementObjectFootprint& footprint,
            SettlementCitizenState& citizens
        );

        [[nodiscard]]
        std::size_t cancelIntersecting(
            SettlementMap& map,
            const SettlementObjectFootprint& footprint,
            SettlementCitizenState& citizens
        );

        [[nodiscard]]
        std::span<const SettlementCommand> commands() const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

        // Player edits rebuild the spatial work board. Completed work is
        // filtered in place, without rebuilding every remaining opportunity.
        std::uint64_t selectionVersion() const noexcept
        {
            return selectionVersion_;
        }
        bool contains(
            const SettlementMap& map,
            SettlementCommandId command,
            SettlementTilePosition tile,
            SettlementObjectId object,
            ConstructionSiteId site
        ) const;

        void pruneInvalid(SettlementMap& map, SettlementCitizenState& citizens);

    private:
        std::uint64_t prunedObjects_ = ~std::uint64_t(0);
        std::uint64_t prunedFeatures_ = ~std::uint64_t(0);
        std::vector<SettlementCommand> commands_;
        IdGenerator<SettlementCommandId> commandIds_;
        std::uint64_t version_ = 0;
        std::uint64_t selectionVersion_ = 0;
        std::size_t pruneCommand_ = 0;
        std::size_t pruneTarget_ = 0;
        bool pruning_ = false;
    };
} // namespace Paladin
