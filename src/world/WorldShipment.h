#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Paladin
{
    enum class ShipmentPhase { Waiting, Outbound, Returning, Completed, Blocked };

    // One route owns one caravan and its physical cargo. The route never owns a
    // second copy in either city's inventory. Paths include both endpoints.
    struct WorldShipment
    {
        static constexpr double MinutesPerTile = 10.0;
        ShipmentId id;
        RealmId owner;
        SettlementId source, destination;
        std::string resource;
        int amount = 1;
        int cargo = 0;
        bool repeating = false;
        bool aiManaged = false;
        ShipmentPhase phase = ShipmentPhase::Waiting;
        std::vector<WorldTilePosition> path;
        std::size_t tileIndex = 0;
        double stepMinutes = 0;
        double deferredMinutes = 0;
        std::uint64_t deliveries = 0;
        int wrapWidth = 0;

        bool moving() const noexcept
        { return phase == ShipmentPhase::Outbound || phase == ShipmentPhase::Returning; }
        bool active() const noexcept { return phase != ShipmentPhase::Completed; }
        WorldTilePosition position() const noexcept
        { return path.empty() ? WorldTilePosition{} : path[tileIndex]; }
        std::size_t nextIndex() const noexcept
        { return phase == ShipmentPhase::Returning ? tileIndex - 1 : tileIndex + 1; }
        double visualX() const noexcept
        {
            if (!moving() || path.size() < 2) return position().x;
            double dx = path[nextIndex()].x - position().x;
            if (wrapWidth > 0 && std::abs(dx) > wrapWidth * .5)
                dx += dx > 0 ? -wrapWidth : wrapWidth;
            return position().x + dx * std::clamp(stepMinutes / MinutesPerTile, 0.0, 1.0);
        }
        double visualY() const noexcept
        {
            return !moving() || path.size() < 2 ? position().y : position().y +
                (path[nextIndex()].y - position().y) * std::clamp(stepMinutes / MinutesPerTile, 0.0, 1.0);
        }
    };
}
