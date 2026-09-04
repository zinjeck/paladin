#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    struct StockpileEntry
    {
        std::string resourceId;
        double amount = 0.0;
    };

    class ResourceStockpile
    {
    public:
        [[nodiscard]]
        double amount(std::string_view resourceId) const noexcept;

        [[nodiscard]]
        bool setAmount(
            std::string resourceId,
            double amount
        );

        [[nodiscard]]
        bool addAmount(
            std::string_view resourceId,
            double amount
        );

        void clear() noexcept;

        [[nodiscard]]
        std::span<const StockpileEntry> entries() const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

    private:
        std::vector<StockpileEntry> entries_;
        std::uint64_t version_ = 0;
    };
}
