#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Paladin
{
    enum class ShipmentPhase
    {
        Waiting,
        Outbound,
        Returning,
        Completed,
        Blocked
    };

    // Presentation/routing contract for future water routes. Existing route
    // creation remains land-only; a visual kind never authorizes a sea path.
    enum class TransportDomain { Land, Water };

    // One route owns one caravan and its physical cargo. The route never owns a
    // second copy in either city's inventory. Paths include both endpoints.
    struct WorldShipment
    {
        static constexpr double MinutesPerTile = 10.0;
        ShipmentId id;
        RealmId owner;
        RealmId buyer; // Empty for ordinary own-settlement logistics.
        std::int64_t unitPrice = 0;
        std::int64_t escrow = 0; // Buyer cash retained until delivery/return.
        SettlementId source, destination;
        SettlementObjectId sourceDepot, destinationDepot;
        double minutesPerTile = MinutesPerTile;
        TransportDomain transportDomain = TransportDomain::Land;
        std::string resource;
        int amount = 1;
        int cargo = 0;
        bool repeating = false;
        // A depot sale can reserve the buyer's money and route before the
        // seller's worker brings its exact batch to the loading counter.
        bool awaitingCollection = false;
        std::uint64_t checkedTerrainRevision = 0;
        bool aiManaged = false;
        ShipmentPhase phase = ShipmentPhase::Waiting;
        std::vector<WorldTilePosition> path;
        std::size_t tileIndex = 0;
        double stepMinutes = 0;
        double deferredMinutes = 0;
        double nextRecoveryMinute = 0;
        std::uint64_t deliveries = 0;
        int wrapWidth = 0;

        bool moving() const noexcept
        {
            return phase == ShipmentPhase::Outbound ||
                   phase == ShipmentPhase::Returning;
        }
        bool active() const noexcept
        {
            return phase != ShipmentPhase::Completed;
        }
        WorldTilePosition position() const noexcept
        {
            return path.empty() ? WorldTilePosition{} : path[std::min(tileIndex,path.size()-1)];
        }
        std::size_t nextIndex() const noexcept
        {
            if (path.empty()) return 0;
            const auto current = std::min(tileIndex,path.size()-1);
            return phase == ShipmentPhase::Returning ? (current ? current-1 : 0)
                                                     : std::min(current+1,path.size()-1);
        }
        double visualX() const noexcept
        {
            if (!moving() || path.size() < 2)
            {
                return position().x;
            }
            double dx = path[nextIndex()].x - position().x;
            if (wrapWidth > 0 && std::abs(dx) > wrapWidth * .5)
            {
                dx += dx > 0 ? -wrapWidth : wrapWidth;
            }
            return position().x +
                   dx * std::clamp(stepMinutes / std::max(.001,minutesPerTile), 0.0, 1.0);
        }
        double visualY() const noexcept
        {
            return !moving() || path.size() < 2
                       ? position().y
                       : position().y + (path[nextIndex()].y - position().y) *
                                            std::clamp(
                                                stepMinutes / std::max(.001,minutesPerTile),
                                                0.0,
                                                1.0
                                            );
        }
    };
} // namespace Paladin
