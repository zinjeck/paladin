#pragma once

#include "core/StrongId.h"
#include <string_view>

namespace Paladin
{
    class World;
    class Settlement;
    struct WorldShipment;
    enum class ShipmentResult
    {
        Success, InvalidSettlement, NotOwned, SameSettlement, InvalidResource,
        InvalidAmount, InsufficientGoods, NoLandRoute, RouteLimit, InvalidRoute
    };
    std::string_view shipmentResultText(ShipmentResult) noexcept;

    class WorldShipmentSystem
    {
    public:
        static constexpr int MaximumShipment = 1000000;
        static constexpr std::size_t MaximumActiveRoutesPerRealm = 64;
        // Display total separately from shippable goods. Private household,
        // construction, market and factory inputs are not requisitioned.
        static double total(const Settlement&, std::string_view resource);
        static int available(const Settlement&, std::string_view resource);
        static ShipmentResult create(World&, RealmId, SettlementId source,
            SettlementId destination, std::string_view resource, int amount,
            bool repeating, ShipmentId* created = nullptr, bool aiManaged = false);
        // In-flight goods finish their delivery/return; stopping cannot delete cargo.
        static ShipmentResult stop(World&, RealmId, ShipmentId);
        static void tick(World&, double minute, double elapsed);
        static bool receive(World&, SettlementId, std::string_view, int amount, double minute);
    private:
        static bool load(World&, WorldShipment&);
    };
}
