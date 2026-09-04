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
        void beginSelection() noexcept;
        void cancelSelection() noexcept;

        [[nodiscard]]
        bool isSelecting() const noexcept;

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
        bool selecting_ = false;
        std::optional<WorldPosition> hoveredPosition_;
    };
}
