#include "world/settlements/SettlementCommerce.h"
#include "world/settlements/SettlementFoodDemand.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    void SettlementCommerce::recordProduction(std::string_view resource, int amount)
    {
        if (amount <= 0) { return; }
        resourceTotals_[std::string(resource)].produced += amount;
        resourceFlows_.record(resource, currentMinute_, amount, 0);
    }

    void SettlementCommerce::recordConsumption(std::string_view resource, int amount)
    {
        if (amount <= 0) { return; }
        resourceTotals_[std::string(resource)].consumed += amount;
        resourceFlows_.record(resource, currentMinute_, 0, amount);
    }

    const std::unordered_map<std::string, ResourceDailyRates>&
    SettlementCommerce::dailyResourceReport(
        const SettlementMap& map, const SettlementCitizenState& people,
        double minute
    ) const
    {
        // HUD reads are pure; refresh at most once per game minute, not per
        // rendered frame. Each settlement owns its own history and snapshot.
        const double now = std::floor(std::max(0.0, minute));
        if (reportMinute_ == now && reportPopulation_ == people.citizens().size())
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
            if (inventory.kind == InventoryKind::Construction) { continue; }
            for (const auto& goods : inventory.goods)
            {
                const auto* definition = SettlementResourceCatalog::definition(goods.resource);
                if (definition && definition->edible && goods.amount > 0)
                {
                    foodStocks[goods.resource] += goods.amount;
                    available += goods.amount;
                }
            }
        }
        for (const auto& definition : SettlementResourceCatalog::definitions())
        {
            auto rates = resourceFlows_.lastDay(definition.id, now);
            if (definition.edible) { eaten += rates.depletion; }
            dailyReport_[std::string(definition.id)] = rates;
        }
        for (const auto& definition : SettlementResourceCatalog::definitions())
        {
            if (!definition.edible) { continue; }
            auto& rates = dailyReport_.at(std::string(definition.id));
            // Observed meal mix includes public/market meals and food eaten
            // while carrying. Before the first meal use available food shares.
            // Shares sum to ONE city demand, never one full demand per food.
            const double share = eaten > 0 ? rates.depletion / eaten
                : available > 0 ? foodStocks[std::string(definition.id)] / available
                                : 0;
            rates.depletion = demand * share;
            rates.foodEstimate = true;
        }
        return dailyReport_;
    }

    void SettlementCommerce::recordFlow(
        InventoryId source,
        InventoryId destination,
        std::string_view resource,
        int amount,
        CitizenId consumer
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
                         std::string(resource);
        auto [it, inserted] = recentFlows_.try_emplace(
            key,
            FrozenFlow{source, destination, consumer, std::string(resource)}
        );
        auto& flow = it->second;
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
        const double workShare = (map.activities.policy.shiftEndMinute -
                                  map.activities.policy.shiftStartMinute) /
                                 1440.0;
        for (const auto& c : people.citizens())
        {
            const bool generalLabor =
                !c.workplaceId ||
                map.activities.pastureWorkerAvailableForGeneralLabor(map, c);
            const bool governmentJob =
                generalLabor &&
                (c.task.kind == CitizenTaskKind::AnimalWork ||
                 c.task.kind == CitizenTaskKind::Build ||
                 c.task.kind == CitizenTaskKind::Gather ||
                 c.task.kind == CitizenTaskKind::Demolish ||
                 c.task.kind == CitizenTaskKind::Haul);
            frozenPayRates_[c.id] = !c.child && (c.workplaceId || governmentJob)
                                        ? policy.dailyWage / 720.0 * workShare
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
                const int amount = std::min(
                    {requested,
                     map.logistics.available(source.id, flow.resource),
                     map.logistics.receivable(destination.id, flow.resource),
                     affordableTradeUnits(source, destination, requested)}
                );
                if (amount > 0 && buyGoods(source, destination, amount))
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
                auto person = std::find_if(
                    people.citizens_.begin(),
                    people.citizens_.end(),
                    [&](const auto& c) { return c.id == flow.consumer; }
                );
                if (person == people.citizens_.end())
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
                    if (price > 0 &&
                        !buyMeal(source.objectId, *person, people, price))
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
        const SettlementInventory& destination
    ) const
    {
        if (!usesMoney())
        {
            return 0;
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
        int requested
    ) const
    {
        const auto price = tradePrice(source, destination);
        const auto payer = businessAccounts_.find(destination.objectId);
        const Money cash =
            payer != businessAccounts_.end()
                ? payer->second
                : ((destination.kind == InventoryKind::Construction ||
                    destination.kind == InventoryKind::Keep)
                       ? treasury->balance
                       : 0);
        return price > 0 ? int(std::min<Money>(requested, cash / price))
                         : requested;
    }
    bool SettlementCommerce::buyGoods(
        const SettlementInventory& source,
        const SettlementInventory& destination,
        int amount
    )
    {
        if (amount <= 0 ||
            affordableTradeUnits(source, destination, amount) < amount)
        {
            return false;
        }
        auto buyer = businessAccounts_.find(destination.objectId),
             seller = businessAccounts_.find(source.objectId);
        Money& from = buyer == businessAccounts_.end() ? treasury->balance
                                                       : buyer->second;
        Money& to = seller == businessAccounts_.end() ? treasury->balance
                                                      : seller->second;
        return transfer(from, to, tradePrice(source, destination) * amount);
    }
    Money SettlementCommerce::mealPrice(
        const SettlementMap& map,
        const SettlementInventory& source
    ) const
    {
        if (!usesMoney())
        {
            return 0;
        }
        const bool stockpileExists = std::any_of(
            map.logistics.inventories().begin(),
            map.logistics.inventories().end(),
            [](const auto& i) { return i.kind == InventoryKind::Stockpile; }
        );
        if (source.kind == InventoryKind::Market)
        {
            return policy.retailFoodPrice +
                   (stockpileExists ? 0 : policy.bypassPremium);
        }
        const bool marketExists = std::any_of(
            map.logistics.inventories().begin(),
            map.logistics.inventories().end(),
            [](const auto& i) { return i.kind == InventoryKind::Market; }
        );
        if (source.kind == InventoryKind::Workplace &&
            (marketExists || stockpileExists))
        {
            return -1;
        }
        if (!marketExists && (source.kind == InventoryKind::Stockpile ||
                              source.kind == InventoryKind::Workplace))
        {
            return policy.retailFoodPrice +
                   policy.bypassPremium *
                       (source.kind == InventoryKind::Stockpile ? 1 : 2);
        }
        return 0; // Public keep/groundpile meals remain emergency relief.
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
        for (auto& c : people.citizens_)
        {
            if (c.health <= 0)
            {
                continue;
            }
            living.emplace(c.id, &c);
            auto [entry, inserted] = citizens_.try_emplace(c.id);
            auto& wallet = entry->second;
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
                map.activities.caregivingAtWorkTime(map, c, minute) ||
                c.task.kind == CitizenTaskKind::AnimalWork ||
                c.task.kind == CitizenTaskKind::Work ||
                c.task.kind == CitizenTaskKind::Build ||
                c.task.kind == CitizenTaskKind::Gather ||
                c.task.kind == CitizenTaskKind::Demolish ||
                c.task.kind == CitizenTaskKind::Haul;
            const auto* assignedWorkplace =
                map.employment().workplace(c.workplaceId);
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
                                    ? policy.dailyWage / 720.0
                                    : (!c.child && c.youngDependents > 0
                                           ? policy.dailyAllowance / 1440.0
                                           : 0);
            wallet.accrued += rate * elapsed;
            const Money due = Money(wallet.accrued);
            wallet.accrued -= double(due);
            Money* employer = &treasury->balance;
            if ((working || inactive_) && c.workplaceId && !pastureCivicLabor)
            {
                if (assignedWorkplace &&
                    businessAccounts_.contains(assignedWorkplace->objectId))
                {
                    employer = &businessAccounts_.at(assignedWorkplace->objectId);
                }
            }
            const Money paid = std::min(*employer, due);
            transfer(*employer, wallet.cash, paid);
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
            if (monetary && !c.child &&
                monetaryMinutes_ >= policy.publicFoodGraceDays * 1440)
            {
                c.publicFoodDissatisfaction = std::clamp(
                    c.publicFoodDissatisfaction +
                        (policy.publicFoodPenaltyPerDay * c.publicMealShare -
                         policy.publicFoodRecoveryPerDay *
                             (1 - c.publicMealShare)) *
                            elapsed / 1440,
                    0.0,
                    policy.maximumPublicFoodPenalty
                );
            }
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
            const Money reserve = policy.operatingReservePerWorker * w.capacity;
            auto& funded = fundedWorkers_[w.objectId];
            if (monetary && w.capacity > funded)
            {
                transfer(
                    treasury->balance,
                    it->second,
                    std::min(
                        treasury->balance,
                        policy.operatingReservePerWorker * (w.capacity - funded)
                    )
                );
            }
            if (monetary)
            {
                funded = w.capacity;
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
