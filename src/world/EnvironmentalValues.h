#pragma once

#include <algorithm>
#include <compare>

namespace Paladin
{
    template<typename Tag>
    class NormalizedWorldValue
    {
    public:
        constexpr NormalizedWorldValue() noexcept = default;

        explicit constexpr NormalizedWorldValue(
            float value
        ) noexcept
            : value_(std::clamp(value, 0.0F, 1.0F))
        {
        }

        [[nodiscard]]
        constexpr float value() const noexcept
        {
            return value_;
        }

        auto operator<=>(
            const NormalizedWorldValue&
        ) const = default;

    private:
        float value_ = 0.0F;
    };

    struct ElevationTag;
    struct TemperatureTag;
    struct RainfallTag;

    using Elevation =
        NormalizedWorldValue<ElevationTag>;

    using Temperature =
        NormalizedWorldValue<TemperatureTag>;

    using Rainfall =
        NormalizedWorldValue<RainfallTag>;
}
