#pragma once
#include "world/entities/attributes/AttributeModifiers.h"
#include <deque>
#include <string>

namespace Paladin
{
    // Settlement-level history: at most 24 hourly buckets, not a history per
    // entity. Applied changes include meals, care and conversations, not just
    // continuous need decay. Population-weighted exposure handles arrivals.
    class SettlementAttributeReport
    {
    public:
        void record(
            double minute,
            double elapsed,
            std::size_t living,
            const AttributeEffectTotals& changes
        );
        std::string tooltip(
            EntityAttribute attribute,
            const EntityAttributes& average
        ) const;
        EntityAttributes average;
        std::size_t living = 0;
        // Public food, taxes, housing pressure and food insecurity, in points.
        std::array<double, 4> happinessPenalties{};

    private:
        struct Bucket
        {
            std::int64_t hour = 0;
            double residentMinutes = 0;
            double elapsed = 0;
            AttributeEffectTotals changes{};
        };
        std::deque<Bucket> history_;
        AttributeEffectTotals activeChanges_{};
        double activeResidentMinutes_ = 0;
    };
} // namespace Paladin
