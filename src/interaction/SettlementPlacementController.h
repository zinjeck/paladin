#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

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
            std::optional<WorldTilePosition> position
        ) noexcept;

        [[nodiscard]]
        std::optional<WorldTilePosition> hoveredPosition() const noexcept;

        [[nodiscard]]
        bool hasValidPlacement(
            const World& world
        ) const noexcept;

        [[nodiscard]]
        bool lockHoveredSelection(
            const World& world
        ) noexcept;

        [[nodiscard]]
        std::optional<WorldTilePosition> lockedPosition() const noexcept;

    private:
        bool selecting_ = false;
        std::optional<WorldTilePosition> hoveredPosition_;
        std::optional<WorldTilePosition> lockedPosition_;
        PolityId ownerPolityId_;
    };
}
