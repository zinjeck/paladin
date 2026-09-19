#pragma once
#include "world/WorldTilePosition.h"
#include <optional>
#include <vector>
namespace Paladin
{
    class WorldGrid;
    // Bounded, cardinal, longitude-wrapped route; water/solid mountains are not
    // traversable. Includes start and destination, also for a zero-length trip.
    std::optional<std::vector<WorldTilePosition>> worldLandRoute(
        const WorldGrid&, WorldTilePosition start, WorldTilePosition destination);
}
