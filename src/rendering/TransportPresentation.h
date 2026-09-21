#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementTradeState.h"

namespace Paladin
{
    inline const char* transportDirection(double dx, double dy)
    {
        return std::abs(dx) > std::abs(dy) ? (dx < 0 ? "west" : "east")
                                           : (dy < 0 ? "north" : "south");
    }
    inline void localTradeCaravans(
        SceneDrawQueue& queue,
        const SceneProjection& view,
        const SceneSpriteLibrary& sprites,
        const SettlementTradeState& trade,
        double minute
    )
    {
        for (const auto& visit : trade.visits)
        {
            if (visit.path.empty() || minute < visit.startMinute ||
                minute >= visit.startMinute + visit.durationMinutes)
            {
                continue;
            }
            const auto [x, y] = visit.position(minute);
            const auto [nx, ny] = visit.position(minute + .1);
            const auto direction = transportDirection(nx - x, ny - y);
            sprites.submit(
                queue,
                view,
                std::string("transport.cart.") + direction,
                x,
                y,
                (std::uint64_t(7) << 60) | visit.shipment.value(),
                .75
            );
            if (visit.importing != visit.returning(minute))
            {
                sprites.placed(
                    queue,
                    view,
                    "resource." + visit.resource,
                    x,
                    y - .35,
                    y + .1,
                    (std::uint64_t(7) << 60) | visit.shipment.value(),
                    10,
                    .4,
                    .3
                );
            }
        }
    }
} // namespace Paladin
