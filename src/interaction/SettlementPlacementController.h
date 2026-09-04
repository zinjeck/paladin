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
        void beginSelection(PolityId ownerPolityId = {}) noexcept;
        void cancelSelection() noexcept;

        [[nodiscard]]
        bool isSelecting() const noexcept;

        [[nodiscard]]
        bool hasLockedSelection() const noexcept;

        [[nodiscard]]
        bool isActive() const noexcept;

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
        bool lockHoveredSelection(
            const World& world
        ) noexcept;

        [[nodiscard]]
        std::optional<WorldPosition> lockedPosition() const noexcept;

    private:
        bool selecting_ = false;
        std::optional<WorldPosition> hoveredPosition_;
        std::optional<WorldPosition> lockedPosition_;
        PolityId ownerPolityId_;
    };
}
