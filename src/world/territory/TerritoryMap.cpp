#include "world/territory/TerritoryMap.h"

#include <algorithm>
#include <stdexcept>

namespace Paladin
{
    TerritoryMap::TerritoryMap(
        std::int32_t width,
        std::int32_t height
    )
        : width_(width),
          height_(height)
    {
        if (width_ <= 0 || height_ <= 0)
        {
            throw std::invalid_argument(
                "TerritoryMap dimensions must be positive."
            );
        }

        controllers_.resize(
            static_cast<std::size_t>(width_)
                * static_cast<std::size_t>(height_)
        );
    }


    std::int32_t TerritoryMap::width() const noexcept
    {
        return width_;
    }


    std::int32_t TerritoryMap::height() const noexcept
    {
        return height_;
    }


    bool TerritoryMap::isValidPosition(
        WorldTilePosition position
    ) const noexcept
    {
        return
            position.x >= 0 &&
            position.y >= 0 &&
            position.x < width_ &&
            position.y < height_;
    }


    PolityId TerritoryMap::controllerAt(
        WorldTilePosition position
    ) const noexcept
    {
        if (!isValidPosition(position))
        {
            return {};
        }

        return controllers_[indexOf(position)];
    }


    bool TerritoryMap::isControlled(
        WorldTilePosition position
    ) const noexcept
    {
        return controllerAt(position).isValid();
    }


    std::size_t TerritoryMap::controlledTileCount() const noexcept
    {
        return controlledTileCount_;
    }


    std::size_t TerritoryMap::controlledTileCount(
        PolityId polityId
    ) const noexcept
    {
        if (!polityId.isValid())
        {
            return 0;
        }

        return static_cast<std::size_t>(
            std::count(
                controllers_.begin(),
                controllers_.end(),
                polityId
            )
        );
    }


    std::span<const WorldTilePosition>
    TerritoryMap::controlledPositions() const noexcept
    {
        return controlledPositions_;
    }


    std::uint64_t TerritoryMap::revision() const noexcept
    {
        return revision_;
    }


    std::size_t TerritoryMap::clearController(
        PolityId polityId
    )
    {
        if (!polityId.isValid())
        {
            return 0;
        }

        std::size_t clearedCount = 0;

        for (PolityId& controller : controllers_)
        {
            if (controller == polityId)
            {
                controller = {};
                ++clearedCount;
            }
        }

        if (clearedCount == 0)
        {
            return 0;
        }

        std::erase_if(
            controlledPositions_,
            [this](WorldTilePosition position)
            {
                return !controllers_[indexOf(position)].isValid();
            }
        );

        controlledTileCount_ -= clearedCount;
        ++revision_;
        return clearedCount;
    }


    bool TerritoryMap::claimIfUncontrolled(
        WorldTilePosition position,
        PolityId polityId
    )
    {
        if (!isValidPosition(position) || !polityId.isValid())
        {
            return false;
        }

        PolityId& controller = controllers_[indexOf(position)];

        if (controller.isValid())
        {
            return controller == polityId;
        }

        controlledPositions_.push_back(position);
        controller = polityId;
        ++controlledTileCount_;
        ++revision_;
        return true;
    }


    std::size_t TerritoryMap::indexOf(
        WorldTilePosition position
    ) const noexcept
    {
        return
            static_cast<std::size_t>(position.y)
                * static_cast<std::size_t>(width_)
            + static_cast<std::size_t>(position.x);
    }
}
