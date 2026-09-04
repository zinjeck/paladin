#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
namespace Paladin
{
struct TimingSamples
{
    std::array<double, 120> samples{};
    std::size_t count = 0, next = 0;
    double last = 0, maximum = 0;
    std::uint64_t slow = 0;
    void add(double ms)
    {
        last = ms;
        maximum = std::max(maximum, ms);
        slow += ms >= 50;
        samples[next++ % samples.size()] = ms;
        count = std::min(count + 1, samples.size());
    }
    std::string text() const
    {
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.begin() + count);
        double sum = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            sum += samples[i];
        }
        std::ostringstream s;
        s << std::fixed << std::setprecision(3) << last << " / "
          << (count ? sum / count : 0) << " / "
          << (count ? sorted[(count - 1) * 95 / 100] : 0) << " / " << maximum;
        return s.str();
    }
};
struct ScopedTiming
{
    TimingSamples& samples;
    std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
    ~ScopedTiming()
    {
        samples.add(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start
            )
                .count()
        );
    }
};
} // namespace Paladin
