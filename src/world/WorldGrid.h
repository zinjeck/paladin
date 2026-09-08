#pragma once

#include "world/TileGrid.h"
#include "world/WorldTilePosition.h"

namespace Paladin
{
    class WorldGrid final : public TileGrid<WorldTilePosition>
    {
    public:
        using TileGrid<WorldTilePosition>::TileGrid;
        std::uint64_t revision() const noexcept
        {
            return revision_;
        }
        // Runtime terrain edits publish once; all projections share this
        // revision.
        bool setTile(WorldTilePosition position, const WorldTile& value)
        {
            auto* target = tile(position);
            if (!target)
            {
                return false;
            }
            *target = value;
            ++revision_;
            return true;
        }
        void terrainChanged() noexcept
        {
            ++revision_;
        }

    private:
        std::uint64_t revision_ = 0;
    };
} // namespace Paladin
