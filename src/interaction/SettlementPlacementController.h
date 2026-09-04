#pragma once

#include "core/StrongId.h"
#include "world/WorldPosition.h"

#include <optional>

namespace Paladin
{
    class World;

    class SettlementPlacementController
    {
    public:
        void setHoveredPosition(
            std::optional<WorldPosition> position
        ) noexcept;

        [[nodiscard]]
        std::optional<WorldPosition> hoveredPosition() const noexcept;

        [[nodiscard]]
        bool hasValidPlacement(
            const World& world
        ) const noexcept;

        [[nodiscard]]
        SettlementId tryFoundSettlement(
            World& world,
            PolityId ownerPolityId
        );

    private:
        std::optional<WorldPosition> hoveredPosition_;
    };
}
