#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Paladin
{
    class World;

    class TerritoryMap
    {
    public:
        TerritoryMap(
            std::int32_t width,
            std::int32_t height
        );

        [[nodiscard]]
        std::int32_t width() const noexcept;

        [[nodiscard]]
        std::int32_t height() const noexcept;

        [[nodiscard]]
        bool isValidPosition(
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        PolityId controllerAt(
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        bool isControlled(
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        std::size_t controlledTileCount() const noexcept;

        [[nodiscard]]
        std::size_t controlledTileCount(
            PolityId polityId
        ) const noexcept;

        [[nodiscard]]
        std::span<const WorldTilePosition>
        controlledPositions() const noexcept;

        [[nodiscard]]
        std::uint64_t revision() const noexcept;

    private:
        friend class TerritoryFoundationSystem;
        friend class World;

        std::size_t clearController(PolityId polityId);

        [[nodiscard]]
        bool claimIfUncontrolled(
            WorldTilePosition position,
            PolityId polityId
        );

        [[nodiscard]]
        std::size_t indexOf(
            WorldTilePosition position
        ) const noexcept;

        std::int32_t width_ = 0;
        std::int32_t height_ = 0;
        std::vector<PolityId> controllers_;
        std::vector<WorldTilePosition> controlledPositions_;
        std::size_t controlledTileCount_ = 0;
        std::uint64_t revision_ = 0;
    };
}
