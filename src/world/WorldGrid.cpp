#include "world/WorldGrid.h"

#include <stdexcept>

namespace Paladin
{
    WorldGrid::WorldGrid(
        std::int32_t width,
        std::int32_t height
    )
        : width_(width),
          height_(height)
    {
        if (width_ <= 0 || height_ <= 0)
        {
            throw std::invalid_argument(
                "WorldGrid dimensions must be positive."
            );
        }

        const std::size_t count =
            static_cast<std::size_t>(width_)
            * static_cast<std::size_t>(height_);

        tiles_.resize(count);
    }


    std::int32_t WorldGrid::width() const noexcept
    {
        return width_;
    }


    std::int32_t WorldGrid::height() const noexcept
    {
        return height_;
    }


    std::size_t WorldGrid::tileCount() const noexcept
    {
        return tiles_.size();
    }


    bool WorldGrid::isValidPosition(
        WorldTilePosition position
    ) const noexcept
    {
        return
            position.x >= 0 &&
            position.y >= 0 &&
            position.x < width_ &&
            position.y < height_;
    }


    WorldTile* WorldGrid::tile(
        WorldTilePosition position
    ) noexcept
    {
        if (!isValidPosition(position))
        {
            return nullptr;
        }

        return &tiles_[indexOf(position)];
    }


    const WorldTile* WorldGrid::tile(
        WorldTilePosition position
    ) const noexcept
    {
        if (!isValidPosition(position))
        {
            return nullptr;
        }

        return &tiles_[indexOf(position)];
    }


    std::size_t WorldGrid::indexOf(
        WorldTilePosition position
    ) const noexcept
    {
        return
            static_cast<std::size_t>(position.y)
            * static_cast<std::size_t>(width_)
            + static_cast<std::size_t>(position.x);
    }
}