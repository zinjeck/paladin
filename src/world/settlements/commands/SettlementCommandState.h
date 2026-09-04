#pragma once

#include "core/StrongId.h"
#include "world/SettlementGrid.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
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
        SettlementObjectFootprint footprint;
        CitizenId assignedCitizenId;
        std::vector<SettlementCommandTarget> targets;
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

        void pruneInvalid(SettlementMap& map, SettlementCitizenState& citizens);

    private:
        std::uint64_t prunedObjects_ = ~std::uint64_t(0);
        std::uint64_t prunedFeatures_ = ~std::uint64_t(0);
        std::vector<SettlementCommand> commands_;
        IdGenerator<SettlementCommandId> commandIds_;
        std::uint64_t version_ = 0;
    };
}
