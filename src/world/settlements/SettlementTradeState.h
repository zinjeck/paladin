#pragma once
#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace Paladin
{
    enum class TradeDirection
    {
        Import,
        Export
    };
    struct SettlementTradeOrder
    {
        SettlementObjectId depot;
        std::string resource;
        TradeDirection direction = TradeDirection::Import;
        int quantity = 10;
        bool enabled = false;
        double nextAttemptMinute = 0;
        ShipmentId shipment;
        std::string status;
        std::uint64_t id = 0;
        bool standing = true;
        bool collectionAuthorized = false;
        bool fulfilled = false;
    };
    struct LocalTradeVisit
    {
        ShipmentId shipment;
        SettlementObjectId depot;
        std::string resource;
        int quantity = 0;
        bool importing = false;
        double startMinute = 0, durationMinutes = 24;
        std::vector<SettlementTilePosition> path;
        bool returning(double minute) const
        {
            return minute - startMinute > durationMinutes * .55;
        }
        std::pair<double, double> position(double minute) const
        {
            if (path.empty())
            {
                return {};
            }
            const double phase =
                std::clamp((minute - startMinute) / durationMinutes, 0., 1.);
            const double travel = phase < .45    ? phase / .45
                                  : phase <= .55 ? 1
                                                 : (1 - phase) / .45;
            const double step = travel * double(path.size() - 1);
            const auto index = std::min(std::size_t(step), path.size() - 1);
            const auto next = std::min(index + 1, path.size() - 1);
            const double weight = step - double(index);
            return {
                path[index].x + (path[next].x - path[index].x) * weight + .5,
                path[index].y + (path[next].y - path[index].y) * weight + .5
            };
        }
    };
    struct SettlementTradeState
    {
        std::vector<SettlementTradeOrder> orders;
        std::vector<LocalTradeVisit> visits;
        std::vector<SettlementId> unreachablePartners;
        std::uint64_t routeTerrainRevision = ~std::uint64_t(0);
        double nextOrderMinute = 0;
        std::size_t orderCursor = 0;
        std::uint64_t nextOrderId = 0;
        int exportTarget(SettlementObjectId depot, std::string_view resource) const
        {
            int target = 0;
            for (const auto& order : orders)
            {
                if (order.enabled && order.collectionAuthorized &&
                    order.depot == depot && order.resource == resource &&
                    order.direction == TradeDirection::Export)
                { target = std::min(1000000, target + order.quantity); }
            }
            return target;
        }
        bool visible(const SettlementTradeOrder& order) const
        { return order.enabled && (order.standing || !order.fulfilled); }
        void expire(double minute)
        {
            std::erase_if(
                visits,
                [minute](const auto& visit)
                { return minute >= visit.startMinute + visit.durationMinutes; }
            );
        }
        SettlementTradeOrder& order(
            SettlementObjectId depot,
            std::string_view resource
        )
        {
            for (auto& order : orders)
            {
                if (order.depot == depot && order.resource == resource)
                {
                    return order;
                }
            }
            orders.push_back({depot, std::string(resource)});
            orders.back().id = ++nextOrderId;
            return orders.back();
        }
    };
} // namespace Paladin
