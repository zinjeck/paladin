#pragma once

#include "simulation/DiplomacySystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include "world/settlements/SettlementFoodDemand.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Paladin
{
    class WorldMarketSystem
    {
    public:
        static ResourceMarketQuote quote(
            const World& world,
            const Settlement& city,
            std::string_view resource
        )
        {
            const auto& state = city.simulationState();
            const auto* local = state.localMap();
            if (!local)
            {
                return state.economy()
                    .quote(state.stockpile(), city.population(), resource);
            }
            const auto* definition =
                SettlementResourceCatalog::definition(resource);
            if (!definition)
            {
                return {};
            }
            const auto& report = local->commerce.dailyResourceReport(
                *local,
                state.citizens(),
                double(world.time().totalGameMinutes())
            );
            const auto found = report.find(std::string(resource));
            double need = found == report.end() ? 0 : found->second.depletion;
            double stock = WorldShipmentSystem::total(city, resource);
            if (definition->edible && !definition->emergencyOnly)
            {
                // Foods substitute for one another; do not purchase a complete
                // diet separately for every edible resource.
                need = 0;
                stock = 0;
                for (const auto& food :
                     SettlementResourceCatalog::definitions())
                {
                    if (!food.edible || food.emergencyOnly)
                    {
                        continue;
                    }
                    if (const auto flow = report.find(std::string(food.id));
                        flow != report.end())
                    {
                        need += flow->second.depletion;
                    }
                    stock += WorldShipmentSystem::total(city, food.id);
                }
                double diet = 0;
                for (const auto& citizen : state.citizens().citizens())
                {
                    diet +=
                        citizenFoodPerDay(citizen, local->activities.policy);
                }
                // Empty pantries have no observed meals, but still need food.
                need = std::max(need, diet);
            }
            if (resource == SettlementResourceTypes::Lumber)
            {
                need = std::max(need, double(city.population()) * .75);
            }
            SettlementEconomy forecast;
            const std::vector<ResourceFlowRate> rates{
                {std::string(resource), 0, need, 0}
            };
            if (!forecast.configure(rates))
            {
                return {};
            }
            ResourceStockpile snapshot;
            if (!snapshot.setAmount(std::string(resource), stock))
            {
                return {};
            }
            auto result = forecast.quote(snapshot, 1, resource);
            result.offered = std::min(
                result.offered,
                double(WorldShipmentSystem::available(city, resource))
            );
            return result;
        }
        static void tick(World& world)
        {
            const double minute = double(world.time().totalGameMinutes());
            for (const auto& record : world.realms())
            {
                if (minute < record.nextMarketMinute)
                {
                    continue;
                }
                auto* buyer = world.realm(record.id());
                buyer->nextMarketMinute =
                    minute + 360 + record.id().value() % 31;
                std::vector<SettlementId> cities;
                for (const auto& city : world.settlements())
                {
                    if (city.ownerRealmId() == buyer->id() &&
                        city.population() > 0 &&
                        WorldShipmentSystem::hasTradeDepot(city))
                    {
                        cities.push_back(city.id());
                    }
                }
                if (cities.empty() || buyer->treasury->balance <= 0)
                {
                    return;
                }
                const auto cityId =
                    cities[buyer->marketCityCursor++ % cities.size()];
                const auto* destination = world.settlement(cityId);
                std::vector<std::pair<double, SettlementId>> candidates;
                for (const auto& city : world.settlements())
                {
                    if (city.ownerRealmId() == buyer->id() ||
                        !WorldShipmentSystem::hasTradeDepot(city))
                    {
                        continue;
                    }
                    const auto* seller = world.realm(city.ownerRealmId());
                    const auto* relation = world.diplomacy().between(
                        buyer->id(),
                        city.ownerRealmId()
                    );
                    if (!seller || !relation || !relation->trading ||
                        relation->atWar)
                    {
                        continue;
                    }
                    const double distance = geographicDistance(
                        destination->position(),
                        city.position(),
                        world.grid().width(),
                        world.grid().height()
                    );
                    if (distance <= DiplomacySystem::RangeRadians)
                    {
                        candidates.emplace_back(distance, city.id());
                    }
                }
                std::sort(candidates.begin(), candidates.end());
                if (candidates.size() > 8)
                {
                    candidates.resize(8);
                }
                struct Offer
                {
                    SettlementId seller;
                    std::string resource;
                    int amount = 0;
                    Money price = 0;
                    double priority = 0;
                } best;
                const auto& state = destination->simulationState();
                for (const auto& resource :
                     SettlementResourceCatalog::definitions())
                {
                    const auto demand = quote(world, *destination, resource.id);
                    double incoming = 0;
                    for (const auto& shipment : world.shipments())
                    {
                        const auto* incomingResource =
                            SettlementResourceCatalog::definition(
                                shipment.resource
                            );
                        const bool interchangeable =
                            state.hasLocalMap() && resource.edible &&
                            !resource.emergencyOnly && incomingResource &&
                            incomingResource->edible &&
                            !incomingResource->emergencyOnly;
                        if (shipment.destination == cityId &&
                            (shipment.resource == resource.id ||
                             interchangeable) &&
                            shipment.phase == ShipmentPhase::Outbound)
                        {
                            incoming += shipment.cargo;
                        }
                    }
                    const double wanted =
                        std::max(0., demand.wanted - incoming);
                    if (wanted < 1)
                    {
                        continue;
                    }
                    for (const auto& [distance, sellerId] : candidates)
                    {
                        const auto* seller = world.settlement(sellerId);
                        const auto supply = quote(world, *seller, resource.id);
                        if (supply.offered < 1 ||
                            supply.unitPrice > demand.unitPrice)
                        {
                            continue;
                        }
                        const Money price =
                            (supply.unitPrice + demand.unitPrice) / 2;
                        const Money budget = buyer->treasury->balance / 20;
                        const int amount = int(std::min(
                            {wanted,
                             supply.offered,
                             double(WorldShipmentSystem::available(
                                 *seller,
                                 resource.id
                             )),
                             double(budget / std::max<Money>(1, price)),
                             1000.}
                        ));
                        const double urgency =
                            (resource.edible ? 4. : 1.) * amount /
                            std::max(1., demand.dailyNeed) / (1 + distance);
                        if (amount > 0 && urgency > best.priority)
                        {
                            best = {
                                sellerId,
                                std::string(resource.id),
                                amount,
                                price,
                                urgency
                            };
                        }
                    }
                }
                if (best.amount > 0)
                {
                    const auto sellerRealm =
                        world.settlement(best.seller)->ownerRealmId();
                    static_cast<void>(WorldShipmentSystem::create(
                        world,
                        sellerRealm,
                        best.seller,
                        cityId,
                        best.resource,
                        best.amount,
                        false,
                        nullptr,
                        true,
                        buyer->id(),
                        best.price
                    ));
                }
                // One buyer and at most one bounded route search per tick.
                return;
            }
        }
    };
} // namespace Paladin
