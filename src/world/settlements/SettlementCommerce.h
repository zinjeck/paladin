#pragma once

#include "core/StrongId.h"
#include "world/settlements/SettlementLogistics.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

namespace Paladin
{
    using Money =
        std::int64_t; // Hundredths of one gold; transfers conserve cash.
    struct IncomeTaxPolicy
    {
        int neutralPercent = 10;
        int maximumPercent = 40;
        int percent = 10;
        void setPercent(int value)
        {
            percent = std::clamp(value, 0, maximumPercent);
        }
    };
    struct Treasury
    {
        Money balance = 0;
        bool moneyEconomyStarted = false;
        bool usesMoney()
        {
            // Realm-wide and one-way: bankruptcy never re-enables free trade.
            moneyEconomyStarted = moneyEconomyStarted || balance > 0;
            return moneyEconomyStarted;
        }
        IncomeTaxPolicy incomeTax;
    };
    inline std::string goldText(Money amount)
    {
        return std::to_string(amount / 100) + "." +
               (amount % 100 < 10 ? "0" : "") + std::to_string(amount % 100);
    }
    struct TreasurySample
    {
        double minute = 0;
        Money realm = 0, markets = 0, households = 0;
    };
    struct CommercePolicy
    {
        Money startingSavings = 0;
        Money retailFoodPrice = 100;
        Money wholesaleFoodPrice = 60;
        Money productionPrice = 40;
        Money bypassPremium = 20;
        Money dailyAllowance = 250;
        Money dailyWage = 250;
        Money dailyChildAllowance = 100;
        Money operatingReservePerWorker = 1600;
        int restockUnitsPerWorker = 8;
        double publicFoodPenaltyPerDay = 2;
        double publicFoodRecoveryPerDay = 4;
        double maximumPublicFoodPenalty = 30;
        double publicFoodGraceDays = 3;
        Money taxProtectedBalance = 200;
        double happinessPerTaxPoint = .5;
        double taxHappinessChangePerDay = 2;
    };
    class SettlementMap;
    class SettlementCitizenState;
    struct SettlementCitizen;
    struct SettlementInventory;
    class SettlementCommerce
    {
    public:
        CommercePolicy policy;
        std::shared_ptr<Treasury> treasury = std::make_shared<Treasury>();
        bool keepFoodSalesEnabled = false;
        IncomeTaxPolicy cityIncomeTax{10, 40, 10};
        bool cityTaxOverride = false;
        bool usesMoney() const
        {
            return treasury->usesMoney();
        }
        int effectiveTaxPercent() const
        {
            return cityTaxOverride ? std::min(
                                         cityIncomeTax.percent,
                                         treasury->incomeTax.percent
                                     )
                                   : treasury->incomeTax.percent;
        }
        void setCityTaxPercent(int rate)
        {
            cityTaxOverride = true;
            cityIncomeTax.setPercent(
                std::min(rate, treasury->incomeTax.percent)
            );
        }
        Money realmTaxCollected = 0, cityTaxCollected = 0;
        double taxHappinessTarget() const
        {
            if (!usesMoney())
            {
                return 0;
            }
            return (treasury->incomeTax.neutralPercent -
                    effectiveTaxPercent()) *
                   policy.happinessPerTaxPoint;
        }
        void update(
            SettlementMap&,
            SettlementCitizenState&,
            double minute,
            double elapsed
        );
        const std::deque<TreasurySample>& history() const
        {
            return history_;
        }
        Money businessTotal() const;
        Money householdTotal() const;
        Money savings(CitizenId id) const;
        Money spendingBalance(
            const SettlementCitizen&,
            const SettlementCitizenState&
        ) const;
        Money businessCash(SettlementObjectId id) const;
        void recordFlow(
            InventoryId source,
            InventoryId destination,
            std::string_view resource,
            int amount,
            CitizenId consumer = {}
        );
        void captureInactive(
            const SettlementMap&,
            const SettlementCitizenState&
        );
        void tickInactive(
            SettlementMap&,
            SettlementCitizenState&,
            double minute,
            double elapsed
        );
        void resumeActive()
        {
            inactive_ = false;
        }
        void setSurplusRecipient(
            SettlementObjectId id,
            std::shared_ptr<Treasury> recipient
        )
        {
            surplusRecipients_[id] = std::move(recipient);
        }
        Money tradePrice(
            const SettlementInventory& source,
            const SettlementInventory& destination
        ) const;
        int affordableTradeUnits(
            const SettlementInventory& source,
            const SettlementInventory& destination,
            int requested
        ) const;
        bool buyGoods(
            const SettlementInventory& source,
            const SettlementInventory& destination,
            int amount
        );
        Money mealPrice(const SettlementMap&, const SettlementInventory&) const;
        bool canBuyMeal(
            const SettlementCitizen&,
            const SettlementCitizenState&,
            Money price = -1
        ) const;
        bool buyMeal(
            SettlementObjectId market,
            const SettlementCitizen&,
            const SettlementCitizenState&,
            Money price = -1
        );
        bool marketOpen(
            const SettlementMap&,
            const SettlementCitizenState&,
            SettlementObjectId market
        ) const;
        void recordMeal(SettlementCitizen&, bool publicFood) const;
        void citizenDeparted(CitizenId);
        // Future depots can settle a funded cross-settlement trade through
        // this same checked cash transfer, after reserving physical goods.
        static bool transfer(Money& from, Money& to, Money amount);

    private:
        struct FrozenFlow
        {
            InventoryId source, destination;
            CitizenId consumer;
            std::string resource;
            double units = 0, lastMinute = 0, rate = 0, accrued = 0;
        };
        std::unordered_map<std::string, FrozenFlow> recentFlows_;
        std::vector<FrozenFlow> frozenFlows_;
        std::unordered_map<CitizenId, double, StrongIdHash> frozenPayRates_;
        std::unordered_map<
            SettlementObjectId,
            std::shared_ptr<Treasury>,
            StrongIdHash>
            surplusRecipients_;
        bool inactive_ = false;
        double currentMinute_ = 0;
        double nextFlowCleanupMinute_ = 0;
        struct Wallet
        {
            Money cash = 0;
            double accrued = 0;
            double supportAccrued = 0;
            bool adultFunded = false;
            Money realmTaxRemainder = 0, cityTaxRemainder = 0;
        };
        std::unordered_map<CitizenId, Wallet, StrongIdHash> citizens_;
        std::unordered_map<SettlementObjectId, Money, StrongIdHash>
            businessAccounts_;
        std::unordered_map<SettlementObjectId, std::uint32_t, StrongIdHash>
            fundedWorkers_;
        CitizenId spouseWallet(
            const SettlementCitizen&,
            const SettlementCitizenState&
        ) const;
        double elapsedMinutes_ = 0;
        double monetaryMinutes_ = 0;
        std::deque<TreasurySample> history_;
    };
} // namespace Paladin
