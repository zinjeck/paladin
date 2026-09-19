#pragma once
#include "world/WorldTilePosition.h"
#include <optional>
#include <vector>
namespace Paladin
{
    class WorldGrid;
    enum class WorldLandMovement { Cardinal, EightWay };

    // Shared authority for both path finding and motion. A diagonal may not
    // squeeze between mountain/water corners, including at the longitude seam.
    bool worldLandStepAllowed(const WorldGrid&, WorldTilePosition from,
        WorldTilePosition to, WorldLandMovement = WorldLandMovement::EightWay) noexcept;
    double worldLandStepDistance(WorldTilePosition from, WorldTilePosition to, int wrapWidth) noexcept;

    // Bounded deterministic A*, longitude-wrapped, no polar wrapping. Includes
    // both endpoints. Caravans retain cardinal routes; armies opt into diagonals.
    std::optional<std::vector<WorldTilePosition>> worldLandRoute(
        const WorldGrid&, WorldTilePosition start, WorldTilePosition destination,
        WorldLandMovement = WorldLandMovement::Cardinal);
}
