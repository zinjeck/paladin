#pragma once

#include "core/StrongId.h"
#include "world/settlements/ResourceFlowHistory.h"
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
        const auto magnitude = amount < 0 ? std::uint64_t(-(amount + 1)) + 1
                                          : std::uint64_t(amount);
        return (amount < 0 ? "-" : "") + std::to_string(magnitude / 100) + "." +
               (magnitude % 100 < 10 ? "0" : "") +
               std::to_string(magnitude % 100);
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
        Money retailLumberPrice = 60;
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
        struct ResourceTotals
        {
            double produced = 0, consumed = 0;
        };
        const std::unordered_map<std::string, ResourceTotals>&
        resourceTotals() const
        {
            return resourceTotals_;
        }
        // Call only after physical production/consumption succeeds. Internal
        // transfers, reservations and refunded construction goods are not flows.
        void recordProduction(std::string_view resource, int amount);
        void recordConsumption(std::string_view resource, int amount);
        // Industrial ingredients and field rations are real depletion, but not
        // meals when estimating the diet of civilians still in this settlement.
        void recordNonMealConsumption(std::string_view resource, int amount);
        const std::unordered_map<std::string, ResourceDailyRates>& dailyResourceReport(
            const SettlementMap&, const SettlementCitizenState&, double minute
        ) const;
        CommercePolicy policy;
        // Per resident, per local solar day; only keep/stockpile food qualifies.
        int foodServings = 1;
        void setFoodServings(int value) { foodServings = std::clamp(value, 0, 10); }
        bool unpaidFoodWarning() const { return currentMinute_ < unpaidFoodUntil_; }
        Money reliefPaid = 0;
        std::uint64_t reliefMeals = 0, unpaidReliefMeals = 0;
        static int sourcePreference(InventoryKind kind)
        {
            return kind == InventoryKind::Market ? 0 :
                   kind == InventoryKind::Groundpile ? 2 : 1;
        }
        bool canAccessMeal(const SettlementMap&, const SettlementInventory&,
                           const SettlementCitizen&, const SettlementCitizenState&) const;
        bool payForMeal(const SettlementMap&, const SettlementInventory&,
                        const SettlementCitizen&, const SettlementCitizenState&);
        std::shared_ptr<Treasury> treasury = std::make_shared<Treasury>();
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
        // Field service is realm-paid, never billed to a captured former city.
        void payFieldSoldier(CitizenId, Treasury& payer, double elapsed);
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
            CitizenId consumer = {},
            bool publicPurchase = false
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
            const SettlementInventory& destination,
            std::string_view resource = {}
        ) const;
        int affordableTradeUnits(
            const SettlementInventory& source,
            const SettlementInventory& destination,
            int requested,
            const SettlementCitizenState* households = nullptr,
            std::string_view resource = {},
            bool publicPurchase = false
        ) const;
        bool buyGoods(
            const SettlementInventory& source,
            const SettlementInventory& destination,
            int amount,
            const SettlementCitizenState* households = nullptr,
            std::string_view resource = {},
            bool publicPurchase = false
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
        double dailyWageFor(const SettlementMap&,const SettlementCitizen&,
                            std::size_t householdAdults,double minute) const;
        std::unordered_map<std::string, ResourceTotals> resourceTotals_;
        ResourceFlowHistory resourceFlows_;
        ResourceFlowHistory nonMealFlows_;
        mutable double reportMinute_ = -1;
        mutable std::size_t reportPopulation_ = 0;
        mutable std::unordered_map<std::string, ResourceDailyRates> dailyReport_;
        struct FrozenFlow
        {
            InventoryId source, destination;
            CitizenId consumer;
            std::string resource;
            double units = 0, lastMinute = 0, rate = 0, accrued = 0;
            bool publicPurchase = false;
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
        double unpaidFoodUntil_ = -1;
        double nextFlowCleanupMinute_ = 0;
        struct Wallet
        {
            Money cash = 0;
            double accrued = 0;
            double supportAccrued = 0;
            bool adultFunded = false;
            std::int64_t reliefDay = -1;
            int reliefServed = 0;
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
