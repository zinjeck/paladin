#pragma once

#include "world/WorldTile.h"
#include "world/WorldTilePosition.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Paladin
{
    class WorldGrid
    {
    public:
        WorldGrid(
            std::int32_t width,
            std::int32_t height
        );

        [[nodiscard]]
        std::int32_t width() const noexcept;

        [[nodiscard]]
        std::int32_t height() const noexcept;

        [[nodiscard]]
        std::size_t tileCount() const noexcept;

        [[nodiscard]]
        bool isValidPosition(
            WorldTilePosition position
        ) const noexcept;

        [[nodiscard]]
        WorldTile* tile(
            WorldTilePosition position
        ) noexcept;

        [[nodiscard]]
        const WorldTile* tile(
            WorldTilePosition position
        ) const noexcept;

    private:
        [[nodiscard]]
        std::size_t indexOf(
            WorldTilePosition position
        ) const noexcept;

        std::int32_t width_ = 0;
        std::int32_t height_ = 0;

        std::vector<WorldTile> tiles_;
    };
}