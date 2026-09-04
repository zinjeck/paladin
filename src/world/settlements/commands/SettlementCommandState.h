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

    struct SettlementCommand
    {
        SettlementCommandId id;
        std::string commandTypeId;
        SettlementObjectFootprint footprint;
        CitizenId assignedCitizenId;
    };

    class SettlementCommandState
    {
    public:
        [[nodiscard]]
        bool add(
            const SettlementGrid& grid,
            std::string_view commandTypeId,
            const SettlementObjectFootprint& footprint,
            SettlementCitizenState& citizens
        );

        [[nodiscard]]
        std::size_t cancelIntersecting(
            const SettlementObjectFootprint& footprint,
            SettlementCitizenState& citizens
        );

        [[nodiscard]]
        std::span<const SettlementCommand> commands() const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

    private:
        std::vector<SettlementCommand> commands_;
        IdGenerator<SettlementCommandId> commandIds_;
        std::uint64_t version_ = 0;
    };
}
