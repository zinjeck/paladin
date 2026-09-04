#pragma once

#include "world/WorldTile.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Paladin
{
    template<typename Position>
    class TileGrid
    {
    public:
        TileGrid(
            std::int32_t width,
            std::int32_t height
        )
            : width_(width),
              height_(height)
        {
            if (width_ <= 0 || height_ <= 0)
            {
                throw std::invalid_argument(
                    "TileGrid dimensions must be positive."
                );
            }

            tiles_.resize(
                static_cast<std::size_t>(width_) *
                    static_cast<std::size_t>(height_)
            );
        }

        [[nodiscard]]
        std::int32_t width() const noexcept
        {
            return width_;
        }

        [[nodiscard]]
        std::int32_t height() const noexcept
        {
            return height_;
        }

        [[nodiscard]]
        std::size_t tileCount() const noexcept
        {
            return tiles_.size();
        }

        [[nodiscard]]
        bool isValidPosition(Position position) const noexcept
        {
            return
                position.x >= 0 &&
                position.y >= 0 &&
                position.x < width_ &&
                position.y < height_;
        }

        [[nodiscard]]
        WorldTile* tile(Position position) noexcept
        {
            return isValidPosition(position)
                ? &tiles_[indexOf(position)]
                : nullptr;
        }

        [[nodiscard]]
        const WorldTile* tile(Position position) const noexcept
        {
            return isValidPosition(position)
                ? &tiles_[indexOf(position)]
                : nullptr;
        }

    private:
        [[nodiscard]]
        std::size_t indexOf(Position position) const noexcept
        {
            return
                static_cast<std::size_t>(position.y) *
                    static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(position.x);
        }

        std::int32_t width_ = 0;
        std::int32_t height_ = 0;
        std::vector<WorldTile> tiles_;
    };
}
