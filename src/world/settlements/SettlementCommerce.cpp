#include "world/settlements/SettlementCommerce.h"
#include "world/settlements/ResourceTradePrices.h"
#include "world/settlements/SettlementFoodDemand.h"
#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/DepotCollection.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    double SettlementCommerce::dailyWageFor(const SettlementMap& map,
        const SettlementCitizen& c,std::size_t householdAdults,double minute) const
    {
        const double food=citizenFoodPerDay(c,map.activities.policy)*policy.retailFoodPrice;
        const double fuel=c.homeId ? SettlementHomeHeating::dailyFuelUnits(minute)*
            (policy.retailLumberPrice+policy.bypassPremium)/std::max<std::size_t>(1,householdAdults) : 0;
        // Net pay covers meals, this adult's share of household heat, and a
        // modest reserve. Higher taxes/shorter shifts never lower this floor.
        return std::max(double(policy.dailyWage),(food+fuel)*1.25/
            std::max(.1,1.0-effectiveTaxPercent()/100.0));
    }
    void SettlementCommerce::recordProduction(
        std::string_view resource,
        int amount
    )
    {
        if (amount <= 0)
        {
            return;
        }
        resourceTotals_[std::string(resource)].produced += amount;
        resourceFlows_.record(resource, currentMinute_, amount, 0);
    }

    void SettlementCommerce::recordConsumption(
        std::string_view resource,
        int amount
    )
    {
        if (amount <= 0)
        {
            return;
        }
        resourceTotals_[std::string(resource)].consumed += amount;
        resourceFlows_.record(resource, currentMinute_, 0, amount);
    }

    void SettlementCommerce::recordNonMealConsumption(
        std::string_view resource,
        int amount
    )
    {
        if (amount <= 0)
        {
            return;
        }
        recordConsumption(resource, amount);
        nonMealFlows_.record(resource, currentMinute_, 0, amount);
    }

    const std::unordered_map<std::string, ResourceDailyRates>&
    SettlementCommerce::dailyResourceReport(
        const SettlementMap& map,
        const SettlementCitizenState& people,
        double minute
    ) const
    {
        // HUD reads are pure; refresh at most once per game minute, not per
        // rendered frame. Each settlement owns its own history and snapshot.
        const double now = std::floor(std::max(0.0, minute));
        if (reportMinute_ == now &&
            reportPopulation_ == people.citizens().size())
        {
            return dailyReport_;
        }
        reportMinute_ = now;
        reportPopulation_ = people.citizens().size();
        dailyReport_.clear();
        double demand = 0, eaten = 0, available = 0;
        for (const auto& citizen : people.citizens())
        {
            demand += citizenFoodPerDay(citizen, map.activities.policy);
        }
        std::unordered_map<std::string, double> foodStocks;
        for (const auto& inventory : map.logistics.inventories())
        {
            if (inventory.kind == InventoryKind::Construction)
            {
                continue;
            }
            for (const auto& goods : inventory.goods)
            {
                const auto* definition =
                    SettlementResourceCatalog::definition(goods.resource);
                if (definition && definition->edible && goods.amount > 0 &&
                    map.logistics.canEat(goods.resource))
                {
                    foodStocks[goods.resource] += goods.amount;
                    available += goods.amount;
                }
            }
        }
        for (const auto& definition : SettlementResourceCatalog::definitions())
        {
            auto rates = resourceFlows_.lastDay(definition.id, now);
            if (definition.edible)
            {
                eaten += std::max(
                    0.0,
                    rates.depletion -
                        nonMealFlows_.lastDay(definition.id, now).depletion
                );
            }
            dailyReport_[std::string(definition.id)] = rates;
        }
        for (const auto& definition : SettlementResourceCatalog::definitions())
        {
            if (!definition.edible)
            {
                continue;
            }
            auto& rates = dailyReport_.at(std::string(definition.id));
            // Observed meal mix includes public/market meals and food eaten
            // while carrying. Before the first meal use available food shares.
            // Shares sum to ONE city demand, never one full demand per food.
            const double inputs =
                nonMealFlows_.lastDay(definition.id, now).depletion;
            const double share =
                eaten > 0 ? std::max(0.0, rates.depletion - inputs) / eaten
                : available > 0
                    ? foodStocks[std::string(definition.id)] / available
                    : 0;
            rates.depletion = demand * share + inputs;
            rates.foodEstimate = true;
        }
        return dailyReport_;
    }

    void SettlementCommerce::recordFlow(
        InventoryId source,
        InventoryId destination,
        std::string_view resource,
        int amount,
        CitizenId consumer, bool publicPurchase
    )
    {
        if (inactive_ || amount <= 0)
        {
            return;
        }
        if (!source && destination)
        {
            recordProduction(resource, amount);
        }
        if (source && consumer && !destination)
        {
            recordConsumption(resource, amount);
        }
        const auto key = std::to_string(source.value()) + ":" +
                         std::to_string(destination.value()) + ":" +
                         std::to_string(consumer.value()) + ":" +
                         std::string(resource) + (publicPurchase ? ":public" : ":private");
        auto [it, inserted] = recentFlows_.try_emplace(
            key,
            FrozenFlow{source, destination, consumer, std::string(resource)}
        );
        auto& flow = it->second;
        flow.publicPurchase = publicPurchase;
        flow.units =
            flow.units *
                std::exp(
                    -std::max(0.0, currentMinute_ - flow.lastMinute) / 1440
                ) +
            amount;
        flow.lastMinute = currentMinute_;
    }
    void SettlementCommerce::captureInactive(
        const SettlementMap& map,
        const SettlementCitizenState& people
    )
    {
        inactive_ = true;
        frozenFlows_.clear();
        frozenPayRates_.clear();
        // A one-day exponential observation window captures the economy at
        // departure. No pathfinding, job selection or invented goods offscreen.
        const double window =
            std::max(1.0, 1440 * (1 - std::exp(-elapsedMinutes_ / 1440)));
        std::erase_if(
            recentFlows_,
            [&](const auto& pair)
            {
                const auto& f = pair.second;
                return (f.source && !map.logistics.inventory(f.source)) ||
                       (f.destination &&
                        !map.logistics.inventory(f.destination));
            }
        );
        for (const auto& [key, observed] : recentFlows_)
        {
            auto flow = observed;
            flow.rate =
                flow.units *
                std::exp(
                    -std::max(0.0, currentMinute_ - flow.lastMinute) / 1440
                ) /
                window;
            flow.accrued = 0;
            frozenFlows_.push_back(std::move(flow));
        }
        // Production, transport, then consumption makes the same fixed flows
        // independent of unordered-map iteration order.
        std::stable_sort(
            frozenFlows_.begin(),
            frozenFlows_.end(),
            [](const auto& a, const auto& b)
            {
                const auto rank = [](const auto& f)
                { return !f.source ? 0 : (f.destination ? 1 : 2); };
                return rank(a) < rank(b);
            }
        );
        std::unordered_map<SettlementObjectId,std::size_t,StrongIdHash> householdAdults;
        for(const auto& c:people.citizens())
            if(!c.child && c.health>0 && !c.militaryDeployed && c.homeId) ++householdAdults[c.homeId];
        for (const auto& c : people.citizens())
        {
            const bool generalLabor =
                !c.workplaceId ||
                map.activities.pastureWorkerAvailableForGeneralLabor(map, c);
            const bool governmentJob =
                generalLabor && (c.task.kind == CitizenTaskKind::AnimalWork ||
                                 c.task.kind == CitizenTaskKind::Build ||
                                 c.task.kind == CitizenTaskKind::Gather ||
                                 c.task.kind == CitizenTaskKind::Demolish ||
                                 c.task.kind == CitizenTaskKind::Haul);
            frozenPayRates_[c.id] = !c.child && (c.workplaceId || governmentJob)
                                        ? dailyWageFor(map,c,householdAdults[c.homeId],currentMinute_) / 1440.0
                                        : (!c.child && c.youngDependents > 0
                                               ? policy.dailyAllowance / 1440.0
                                               : 0);
        }
    }
    void SettlementCommerce::tickInactive(
        SettlementMap& map,
        SettlementCitizenState& people,
        double minute,
        double elapsed
    )
    {
        if (!inactive_)
        {
            captureInactive(map, people);
        }
        update(map, people, minute, elapsed);
        advanceInactiveIndustry(map, people, minute, elapsed);
        for (auto& flow : frozenFlows_)
        {
            flow.accrued += flow.rate * elapsed;
            const int requested =
                int(std::min(1000000.0, std::floor(flow.accrued)));
            if (requested <= 0)
            {
                continue;
            }
            flow.accrued -= requested;
            const auto* from = map.logistics.inventory(flow.source);
            const auto* to = map.logistics.inventory(flow.destination);
            const auto* processor =
                to ? map.objectState().completedObject(to->objectId) : nullptr;
            // Recipes run above. Never replay their observed output as free
            // production or purchase the same processor's inputs a second time.
            if (processor && isIndustry(processor->objectTypeId))
            {
                continue;
            }
            if (from &&
                !map.logistics
                     .mayExport(map.objectState(), *from, flow.resource) &&
                flow.destination)
            {
                continue;
            }
            if (!flow.source && to)
            {
                const int amount =
                    std::min(requested, map.logistics.freeSpace(to->id));
                if (map.logistics.add(to->id, flow.resource, amount, minute))
                {
                    recordProduction(flow.resource, amount);
                }
            }
            else if (from && to)
            {
                const auto source = *from, destination = *to;
                if (!map.logistics.importsMaySupply(source, destination.kind)) { continue; }
                if (destination.kind == InventoryKind::TradeDepot &&
                    !DepotCollection::source(map, source, flow.resource)) { continue; }
                const int authorized = destination.kind == InventoryKind::TradeDepot
                    ? DepotCollection::needed(map, destination, flow.resource)
                    : requested;
                const int amount = std::min(
                    {requested, authorized,
                     map.logistics.available(source.id, flow.resource),
                     map.logistics.receivable(destination.id, flow.resource),
                     affordableTradeUnits(
                         source,
                         destination,
                         requested,
                         &people,
                         flow.resource, flow.publicPurchase
                     )}
                );
                if (amount > 0 && buyGoods(
                                      source,
                                      destination,
                                      amount,
                                      &people,
                                      flow.resource, flow.publicPurchase
                                  ))
                {
                    map.logistics.moveAvailable(
                        source.id,
                        destination.id,
                        flow.resource,
                        amount
                    );
                    if (destination.siteId)
                    {
                        map.objectState().deliverMaterials(
                            destination.siteId,
                            flow.resource,
                            map.logistics.inventory(destination.id)
                                ->amount(flow.resource)
                        );
                    }
                }
            }
            else if (from && flow.consumer)
            {
                auto* person = people.mutableCitizen(flow.consumer);
                if (!person ||
                    person->militaryDeployed ||
                    !map.logistics.canEat(flow.resource))
                {
                    continue;
                }
                const auto source = *from;
                const Money price = mealPrice(map, source);
                if (price < 0)
                {
                    continue;
                }
                const int meals = std::min(
                    requested,
                    map.logistics.available(source.id, flow.resource)
                );
                for (int meal = 0; meal < meals; ++meal)
                {
                    if (!map.logistics.canEat(flow.resource))
                    {
                        break;
                    }
                    if (!payForMeal(map, source, *person, people))
                    {
                        break;
                    }
                    map.logistics
                        .moveAvailable(source.id, {}, flow.resource, 1);
                    recordMeal(*person, price == 0);
                    recordConsumption(flow.resource, 1);
                }
            }
        }
    }
    Money SettlementCommerce::tradePrice(
        const SettlementInventory& source,
        const SettlementInventory& destination,
        std::string_view resource
    ) const
    {
        if (!usesMoney())
        {
            return 0;
        }
        if (destination.kind == InventoryKind::Home)
        {
            return source.kind == InventoryKind::Keep ||
                           source.kind == InventoryKind::Groundpile
                       ? 0
                       : policy.retailLumberPrice +
                             (source.kind == InventoryKind::Market
                                  ? 0
                                  : policy.bypassPremium);
        }
        if (destination.kind == InventoryKind::TradeDepot)
        {
            // Preparing the realm's export cargo is a goods transfer. The
            // foreign buyer pays on the shipment; staging requires no gold.
            return 0;
        }
        if (source.kind == InventoryKind::Market)
        {
            return resource == "lumber" ? policy.retailLumberPrice
                                        : std::max(
                                              policy.retailFoodPrice,
                                              resourceTradeBasePrice(resource)
                                          );
        }
        if (source.kind == InventoryKind::TradeImports)
        {
            return std::max<Money>(1, resourceTradeBasePrice(resource) * 3 / 4);
        }
        if (!resource.empty())
        {
            const auto base = resourceTradeBasePrice(resource);
            const auto producerPrice = std::max<Money>(1, base * 28 / 100);
            const auto wholesalePrice = std::max<Money>(1, base * 40 / 100);
            if (destination.kind == InventoryKind::Construction)
            {
                return source.kind == InventoryKind::Keep ||
                               source.kind == InventoryKind::Groundpile
                           ? 0
                       : source.kind == InventoryKind::Workplace
                           ? producerPrice
                           : wholesalePrice;
            }
            if (destination.kind == InventoryKind::Workplace ||
                destination.kind == InventoryKind::Stockpile)
            {
                return source.kind == InventoryKind::Groundpile
                           ? std::max<Money>(1, base / 5)
                       : source.kind == InventoryKind::Workplace
                           ? producerPrice
                           : wholesalePrice;
            }
            if (destination.kind == InventoryKind::Market)
            {
                return std::max<Money>(
                    1,
                    base * (source.kind == InventoryKind::Stockpile ? 60 : 45) /
                        100
                );
            }
        }
        if (destination.kind == InventoryKind::Construction ||
            destination.kind == InventoryKind::Keep)
        {
            return source.kind == InventoryKind::Workplace
                       ? policy.productionPrice
                       : (source.kind == InventoryKind::Stockpile
                              ? policy.wholesaleFoodPrice
                              : 0);
        }
        if (destination.kind == InventoryKind::Workplace)
        {
            // Processors and barracks buy inputs from existing owners. The
            // destination used to fall through to a zero-price transfer.
            return source.kind == InventoryKind::Workplace
                       ? policy.productionPrice
                       : policy.wholesaleFoodPrice;
        }
        if (destination.kind == InventoryKind::Stockpile)
        {
            return policy.productionPrice +
                   (source.kind == InventoryKind::Groundpile
                        ? policy.bypassPremium
                        : 0);
        }
        if (destination.kind == InventoryKind::Market)
        {
            return policy.wholesaleFoodPrice +
                   (source.kind == InventoryKind::Stockpile
                        ? 0
                        : policy.bypassPremium *
                              (source.kind == InventoryKind::Groundpile ? 2
                                                                        : 1));
        }
        return 0;
    }
    int SettlementCommerce::affordableTradeUnits(
        const SettlementInventory& source,
        const SettlementInventory& destination,
        int requested,
        const SettlementCitizenState* households,
        std::string_view resource, bool publicPurchase
    ) const
    {
        if (destination.kind == InventoryKind::TradeDepot &&
            (source.kind == InventoryKind::Market ||
             source.kind == InventoryKind::TradeImports ||
             source.kind == InventoryKind::Keep))
        {
            return 0;
        }
        const auto price = tradePrice(source, destination, resource);
        if (destination.kind == InventoryKind::Home && price > 0)
        {
            if (!households)
            {
                return 0;
            }
            Money cash = 0;
            for (const auto& person : households->citizens())
            {
                if (person.homeId == destination.objectId && !person.child &&
                    person.health > 0 && !person.militaryDeployed)
                {
                    cash += std::min(
                        savings(person.id),
                        std::numeric_limits<Money>::max() - cash
                    );
                }
            }
            return int(std::min<Money>(requested, cash / price));
        }
        const bool civic = (publicPurchase && destination.kind != InventoryKind::Home &&
            destination.kind != InventoryKind::Market) || destination.kind == InventoryKind::Construction ||
            destination.kind == InventoryKind::Keep ||
            (source.kind == InventoryKind::TradeImports && destination.kind == InventoryKind::Stockpile);
        const auto payer = (civic || destination.kind == InventoryKind::TradeDepot)
                               ? businessAccounts_.end()
                               : businessAccounts_.find(destination.objectId);
        const Money cash =
            payer != businessAccounts_.end()
                ? payer->second
                : ((destination.kind == InventoryKind::Construction ||
                    destination.kind == InventoryKind::Keep ||
                    destination.kind == InventoryKind::TradeDepot || civic)
                       ? treasury->balance
                       : 0);
        return price > 0 ? int(std::min<Money>(requested, cash / price))
                         : requested;
    }
    bool SettlementCommerce::buyGoods(
        const SettlementInventory& source,
        const SettlementInventory& destination,
        int amount,
        const SettlementCitizenState* households,
        std::string_view resource, bool publicPurchase
    )
    {
        if (amount <= 0 || affordableTradeUnits(
                               source,
                               destination,
                               amount,
                               households,
                               resource, publicPurchase
                           ) < amount)
        {
            return false;
        }
        const bool civic = (publicPurchase && destination.kind != InventoryKind::Home &&
            destination.kind != InventoryKind::Market) || destination.kind == InventoryKind::Construction ||
            destination.kind == InventoryKind::Keep ||
            (source.kind == InventoryKind::TradeImports && destination.kind == InventoryKind::Stockpile);
        auto buyer = (civic || destination.kind == InventoryKind::TradeDepot)
                         ? businessAccounts_.end()
                         : businessAccounts_.find(destination.objectId),
             seller = (source.kind == InventoryKind::TradeDepot)
                          ? businessAccounts_.end()
                          : businessAccounts_.find(source.objectId);
        Money& from = buyer == businessAccounts_.end() ? treasury->balance
                                                       : buyer->second;
        Money& to = seller == businessAccounts_.end() ? treasury->balance
                                                      : seller->second;
        const Money price = tradePrice(source, destination, resource);
        if (price > std::numeric_limits<Money>::max() / amount)
        {
            return false;
        }
        Money payment = price * amount;
        if (destination.kind == InventoryKind::Home && payment > 0)
        {
            if (!households || to > std::numeric_limits<Money>::max() - payment)
            {
                return false;
            }
            for (const auto& person : households->citizens())
            {
                if (person.homeId != destination.objectId || person.child ||
                    person.health <= 0 || person.militaryDeployed)
                {
                    continue;
                }
                const auto share = std::min(payment, savings(person.id));
                if (share > 0)
                {
                    transfer(citizens_.at(person.id).cash, to, share);
                }
                payment -= share;
                if (!payment)
                {
                    return true;
                }
            }
            return false;
        }
        return transfer(from, to, payment);
    }
    Money SettlementCommerce::mealPrice(
        const SettlementMap&,
        const SettlementInventory& source
    ) const
    {
        // Availability and price never depend on another building existing.
        if (source.kind == InventoryKind::TradeDepot ||
            source.kind == InventoryKind::Construction || source.kind == InventoryKind::Home)
        { return -1; }
        return usesMoney() ? policy.retailFoodPrice : 0;
    }
    bool SettlementCommerce::canAccessMeal(
        const SettlementMap& map, const SettlementInventory& source,
        const SettlementCitizen& c, const SettlementCitizenState& people) const
    {
        const auto price = mealPrice(map, source);
        if (price < 0 || c.health <= 0) { return false; }
        if (price == 0 || canBuyMeal(c, people, price)) { return true; }
        if (source.kind != InventoryKind::Keep && source.kind != InventoryKind::Stockpile)
        { return false; }
        const auto day = std::int64_t(std::floor((currentMinute_ +
            map.activities.policy.solarTimeOffsetMinutes) / 1440));
        const auto it = citizens_.find(c.id);
        const int served = it != citizens_.end() && it->second.reliefDay == day
            ? it->second.reliefServed : 0;
        return served < std::clamp(foodServings, 0, 10);
    }
    bool SettlementCommerce::payForMeal(
        const SettlementMap& map, const SettlementInventory& source,
        const SettlementCitizen& c, const SettlementCitizenState& people)
    {
        if (!canAccessMeal(map, source, c, people)) { return false; }
        const auto price = mealPrice(map, source);
        if (price == 0) { return true; }
        if (canBuyMeal(c, people, price)) { return buyMeal(source.objectId,c,people,price); }
        auto& wallet = citizens_[c.id];
        const auto day = std::int64_t(std::floor((currentMinute_ +
            map.activities.policy.solarTimeOffsetMinutes) / 1440));
        if (wallet.reliefDay != day) { wallet.reliefDay = day; wallet.reliefServed = 0; }
        ++wallet.reliefServed; ++reliefMeals;
        // Public keep provisions still compensate the food supply account.
        Money& recipient = businessAccounts_[source.objectId];
        const Money paid = std::min(price, std::max<Money>(0,treasury->balance));
        transfer(treasury->balance,recipient,paid);
        reliefPaid += paid;
        if (paid < price) { ++unpaidReliefMeals; unpaidFoodUntil_ = currentMinute_ + 1440; }
        return true;
    }
    bool SettlementCommerce::transfer(Money& from, Money& to, Money amount)
    {
        if (amount < 0 || from < amount ||
            to > std::numeric_limits<Money>::max() - amount)
        {
            return false;
        }
        from -= amount;
        to += amount;
        return true;
    }
    Money SettlementCommerce::savings(CitizenId id) const
    {
        const auto it = citizens_.find(id);
        return it == citizens_.end() ? 0 : it->second.cash;
    }
    Money SettlementCommerce::businessCash(SettlementObjectId id) const
    {
        const auto it = businessAccounts_.find(id);
        return it == businessAccounts_.end() ? 0 : it->second;
    }
    Money SettlementCommerce::businessTotal() const
    {
        Money result = 0;
        for (const auto& [id, value] : businessAccounts_)
        {
            result += value;
        }
        return result;
    }
    Money SettlementCommerce::householdTotal() const
    {
        Money result = 0;
        for (const auto& [id, value] : citizens_)
        {
            result += value.cash;
        }
        return result;
    }
    CitizenId SettlementCommerce::spouseWallet(
        const SettlementCitizen& c,
        const SettlementCitizenState& state
    ) const
    {
        const auto* spouse = state.citizen(c.spouseId);
        return !c.child && c.health > 0 && spouse && !spouse->child &&
                       spouse->health > 0 && spouse->id != c.id &&
                       spouse->spouseId == c.id
                   ? spouse->id
                   : CitizenId{};
    }
    Money SettlementCommerce::spendingBalance(
        const SettlementCitizen& c,
        const SettlementCitizenState& state
    ) const
    {
        if (c.child || c.health <= 0)
        {
            return 0;
        }
        return savings(c.id) + savings(spouseWallet(c, state));
    }
    bool SettlementCommerce::canBuyMeal(
        const SettlementCitizen& c,
        const SettlementCitizenState& state,
        Money price
    ) const
    {
        if (!usesMoney())
        {
            return !c.child && c.health > 0;
        }
        return !c.child && c.health > 0 &&
               spendingBalance(c, state) >=
                   (price < 0 ? policy.retailFoodPrice : price);
    }
    bool SettlementCommerce::buyMeal(
        SettlementObjectId market,
        const SettlementCitizen& c,
        const SettlementCitizenState& state,
        Money price
    )
    {
        if (!usesMoney())
        {
            return !c.child && c.health > 0;
        }
        if (price < 0)
        {
            price = policy.retailFoodPrice;
        }
        if (!canBuyMeal(c, state, price))
        {
            return false;
        }
        const auto spouse = spouseWallet(c, state);
        const auto it = businessAccounts_.find(market);
        Money& seller =
            it == businessAccounts_.end() ? treasury->balance : it->second;
        if (seller > std::numeric_limits<Money>::max() - price)
        {
            return false;
        }
        const Money ownShare = std::min(price, savings(c.id));
        if (ownShare > 0 &&
            !transfer(citizens_.at(c.id).cash, seller, ownShare))
        {
            return false;
        }
        return price == ownShare ||
               (spouse &&
                transfer(citizens_.at(spouse).cash, seller, price - ownShare));
    }
    bool SettlementCommerce::marketOpen(
        const SettlementMap& map,
        const SettlementCitizenState& people,
        SettlementObjectId market
    ) const
    {
        const auto* object = map.objectState().completedObject(market);
        if (!object || object->objectTypeId != SettlementObjectTypes::Market)
        {
            return false;
        }
        return std::any_of(
            people.citizens().begin(),
            people.citizens().end(),
            [&](const auto& c)
            {
                return c.health > 0 && c.task.kind == CitizenTaskKind::Work &&
                       c.task.object == market && c.path.empty() &&
                       c.tilePosition == c.destination;
            }
        );
    }
    void SettlementCommerce::recordMeal(
        SettlementCitizen& c,
        bool publicFood
    ) const
    {
        c.publicMealShare =
            usesMoney() ? c.publicMealShare * .8 + (publicFood ? .2 : 0) : 0;
    }
    void SettlementCommerce::citizenDeparted(CitizenId id)
    {
        auto it = citizens_.find(id);
        if (it != citizens_.end())
        {
            transfer(it->second.cash, treasury->balance, it->second.cash);
            citizens_.erase(it);
        }
    }
    void SettlementCommerce::payFieldSoldier(
        CitizenId id,
        Treasury& payer,
        double elapsed
    )
    {
        if (!id || !std::isfinite(elapsed) || elapsed <= 0 ||
            !payer.usesMoney())
        {
            return;
        }
        const CommercePolicy fieldPolicy;
        auto& wallet = citizens_[id];
        wallet.accrued += double(fieldPolicy.dailyWage) * elapsed / 1440.;
        const Money due = Money(wallet.accrued);
        wallet.accrued -= double(due);
        const Money paid = std::min(payer.balance, due);
        transfer(payer.balance, wallet.cash, paid);
        const Money taxable = std::min(
            paid,
            std::max<Money>(0, wallet.cash - fieldPolicy.taxProtectedBalance)
        );
        const Money hundredths =
            taxable * payer.incomeTax.percent + wallet.realmTaxRemainder;
        wallet.realmTaxRemainder = hundredths % 100;
        transfer(wallet.cash, payer.balance, hundredths / 100);
    }
    void SettlementCommerce::update(
        SettlementMap& map,
        SettlementCitizenState& people,
        double minute,
        double elapsed
    )
    {
        const bool monetary = usesMoney();
        elapsedMinutes_ += elapsed;
        if (monetary)
        {
            monetaryMinutes_ += elapsed;
        }
        currentMinute_ = minute + elapsed;
        if (!inactive_ && currentMinute_ >= nextFlowCleanupMinute_)
        {
            nextFlowCleanupMinute_ = currentMinute_ + 240;
            std::erase_if(
                recentFlows_,
                [&](const auto& pair)
                {
                    const auto& f = pair.second;
                    return (f.source && !map.logistics.inventory(f.source)) ||
                           (f.destination &&
                            !map.logistics.inventory(f.destination)) ||
                           currentMinute_ - f.lastMinute > 2 * 1440;
                }
            );
        }
        std::unordered_map<CitizenId, const SettlementCitizen*, StrongIdHash>
            living;
        living.reserve(people.citizens().size());
        std::unordered_map<SettlementObjectId,std::size_t,StrongIdHash> householdAdults;
        std::unordered_map<WorkplaceId,std::uint32_t,StrongIdHash> employees;
        for(const auto& c:people.citizens())
        {
            if(c.health<=0 || c.militaryDeployed) continue;
            if(c.workplaceId) ++employees[c.workplaceId];
            if(!c.child && c.homeId) ++householdAdults[c.homeId];
        }
        Money* publicFoodPayroll=nullptr;
        for(const auto& object:map.objectState().completedObjects())
        {
            if(object.objectTypeId!=SettlementObjectTypes::CityKeep) continue;
            const auto account=businessAccounts_.find(object.id);
            if(account!=businessAccounts_.end()) publicFoodPayroll=&account->second;
            break;
        }
        for (auto& c : people.citizens_)
        {
            if (c.health <= 0)
            {
                continue;
            }
            living.emplace(c.id, &c);
            auto [entry, inserted] = citizens_.try_emplace(c.id);
            auto& wallet = entry->second;
            // The record remains for identity/family/savings continuity, but
            // an expeditionary soldier isn't this city's employee or taxpayer.
            // MilitarySystem pays from the unit's realm and handles field food.
            if (c.militaryDeployed)
            {
                continue;
            }
            if (monetary && !wallet.adultFunded && !c.child)
            {
                wallet.adultFunded = true;
                transfer(
                    treasury->balance,
                    wallet.cash,
                    std::min(treasury->balance, policy.startingSavings)
                );
            }
            const bool working =
                (!c.child && c.workplaceId && map.activities.policy.isWorkTime(minute)) ||
                map.activities.caregivingAtWorkTime(map, c, minute) ||
                c.task.kind == CitizenTaskKind::AnimalWork ||
                c.task.kind == CitizenTaskKind::Work ||
                c.task.kind == CitizenTaskKind::Build ||
                c.task.kind == CitizenTaskKind::Gather ||
                c.task.kind == CitizenTaskKind::Demolish ||
                c.task.kind == CitizenTaskKind::Haul;
            const auto* assignedWorkplace =
                map.employment().workplace(c.workplaceId);
            const bool foodWorker = assignedWorkplace &&
                (assignedWorkplace->objectTypeId == SettlementObjectTypes::FishingGrounds ||
                 assignedWorkplace->objectTypeId == SettlementObjectTypes::WheatFarm ||
                 assignedWorkplace->objectTypeId == SettlementObjectTypes::Pastureland ||
                 assignedWorkplace->objectTypeId == SettlementObjectTypes::Bakery);
            const bool pastureCivicLabor =
                assignedWorkplace &&
                assignedWorkplace->objectTypeId ==
                    SettlementObjectTypes::Pastureland &&
                c.task.kind != CitizenTaskKind::Work;
            // Businesses pay their own active staff and retain operating
            // reserves. An empty-pasture employee doing the same civic work as
            // an unemployed citizen is treasury-paid for that work instead.
            const double rate = !monetary   ? 0
                                : inactive_ ? frozenPayRates_[c.id]
                                : !c.child && working
                                    ? dailyWageFor(map,c,householdAdults[c.homeId],minute) /
                                        std::max(1.0,double(map.activities.policy.shiftEndMinute-map.activities.policy.shiftStartMinute))
                                    : (!c.child && c.youngDependents > 0
                                           ? policy.dailyAllowance / 1440.0
                                           : 0);
            wallet.accrued += rate * elapsed;
            const Money due = Money(wallet.accrued);
            Money* employer = &treasury->balance;
            if ((working || inactive_) && c.workplaceId && !pastureCivicLabor)
            {
                if (assignedWorkplace &&
                    businessAccounts_.contains(assignedWorkplace->objectId))
                {
                    employer =
                        &businessAccounts_.at(assignedWorkplace->objectId);
                }
            }
            Money paid = 0;
            // Keep relief payments are earmarked for the food sector. Release
            // them into real payroll instead of trapping circulating cash in
            // a public storage account with no employees of its own.
            if(foodWorker && publicFoodPayroll)
            {
                paid=std::min(*publicFoodPayroll,due);
                transfer(*publicFoodPayroll,wallet.cash,paid);
            }
            const Money employerPaid=std::min(*employer,due-paid);
            transfer(*employer,wallet.cash,employerPaid);
            paid+=employerPaid;
            if (employer != &treasury->balance && paid < due)
            {
                const Money support = std::min(treasury->balance, due-paid);
                transfer(treasury->balance,wallet.cash,support); paid += support;
            }
            wallet.accrued -= double(paid);
            // Withhold only from wages actually paid. Savings, allowances,
            // child support and starting grants are not income-taxed.
            if (!c.child &&
                (working ||
                 (inactive_ && (c.workplaceId || c.youngDependents == 0))) &&
                paid > 0)
            {
                const Money taxable = std::min(
                    paid,
                    std::max<Money>(0, wallet.cash - policy.taxProtectedBalance)
                );
                const auto collect =
                    [&](int percent, Money& remainder, Money& total)
                {
                    const Money hundredths = taxable * percent + remainder;
                    const Money tax = hundredths / 100;
                    remainder = hundredths % 100;
                    if (transfer(wallet.cash, treasury->balance, tax))
                    {
                        total += tax;
                    }
                };
                collect(
                    effectiveTaxPercent(),
                    wallet.realmTaxRemainder,
                    cityTaxOverride ? cityTaxCollected : realmTaxCollected
                );
            }
            // A persistent, gradual sentiment modifier cannot be erased by
            // the ordinary "fed and housed" happiness recovery each tick.
            const double previousTaxMood = c.taxHappinessAdjustment;
            const double target = c.child ? 0 : taxHappinessTarget();
            const double step =
                policy.taxHappinessChangePerDay * elapsed / 1440;
            c.taxHappinessAdjustment +=
                std::clamp(target - previousTaxMood, -step, step);
            c.modifyAttributes(
                {{AttributeEffect::Taxes,
                  c.taxHappinessAdjustment - previousTaxMood}}
            );
            c.publicFoodDissatisfaction = std::clamp(c.publicFoodDissatisfaction +
                ((monetary && foodWorker && unpaidFoodWarning()) ? policy.publicFoodPenaltyPerDay
                    : -policy.publicFoodRecoveryPerDay) * elapsed / 1440,
                0.0, policy.maximumPublicFoodPenalty);
        }
        // Child support goes to a living parent, so dependent children never
        // need a job or an independently replenished wallet to afford food.
        for (const auto& c : people.citizens())
        {
            if (!monetary || !c.child || c.health <= 0)
            {
                continue;
            }
            const auto mother = living.find(c.motherId);
            const auto father = living.find(c.fatherId);
            const auto parent = mother != living.end() ? mother : father;
            if (parent == living.end())
            {
                continue;
            }
            auto& wallet = citizens_[parent->first];
            wallet.supportAccrued +=
                policy.dailyChildAllowance * elapsed / 1440;
            const Money due = Money(wallet.supportAccrued);
            wallet.supportAccrued -= double(due);
            transfer(
                treasury->balance,
                wallet.cash,
                std::min(treasury->balance, due)
            );
        }
        for (const auto& w : map.employment().workplaces())
        {
            if (!w.operational)
            {
                continue;
            }
            auto [it, created] = businessAccounts_.try_emplace(w.objectId, 0);
            const auto workers=employees[w.id];
            const Money reserve = policy.operatingReservePerWorker * workers;
            auto& funded = fundedWorkers_[w.objectId];
            if (monetary && workers > funded)
            {
                transfer(
                    treasury->balance,
                    it->second,
                    std::min(
                        treasury->balance,
                        policy.operatingReservePerWorker * (workers - funded)
                    )
                );
            }
            if (monetary)
            {
                funded = workers;
            }
            if (it->second > reserve)
            {
                const auto recipient = surplusRecipients_.find(w.objectId);
                auto& recipientBalance =
                    recipient != surplusRecipients_.end() && recipient->second
                        ? recipient->second->balance
                        : treasury->balance;
                transfer(it->second, recipientBalance, it->second - reserve);
            }
        }
        for (auto it = businessAccounts_.begin();
             it != businessAccounts_.end();)
        {
            if (!map.objectState().completedObject(it->first))
            {
                transfer(it->second, treasury->balance, it->second);
                fundedWorkers_.erase(it->first);
                it = businessAccounts_.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (history_.empty() ||
            std::floor(minute / 240) > std::floor(history_.back().minute / 240))
        {
            history_.push_back(
                {minute, treasury->balance, businessTotal(), householdTotal()}
            );
            while (history_.size() > 97)
            {
                history_.pop_front();
            }
        }
    }
} // namespace Paladin
