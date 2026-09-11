#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Paladin
{
    struct ResourceDailyRates
    {
        double production = 0;
        double depletion = 0;
        bool foodEstimate = false;
    };

    // Observed physical creation/use, NOT changes in a storage container.
    // Events in one game minute coalesce. At most 1,441 samples per resource
    // survive, independent of population, frame rate and simulation speed.
    class ResourceFlowHistory
    {
        struct Sample
        {
            double minute = 0, produced = 0, consumed = 0;
        };
        std::unordered_map<std::string, std::deque<Sample>> samples_;

    public:
        void record(std::string_view resource, double minute,
                    double produced, double consumed)
        {
            if (resource.empty() || !std::isfinite(minute) || minute < 0 ||
                !std::isfinite(produced) || !std::isfinite(consumed) ||
                produced < 0 || consumed < 0 || (produced == 0 && consumed == 0))
            {
                return;
            }
            const double bucket = std::floor(minute);
            auto& samples = samples_[std::string(resource)];
            const double newest = samples.empty() ? bucket
                                                 : std::max(bucket, samples.back().minute);
            while (!samples.empty() && samples.front().minute < newest - 1440)
            {
                samples.pop_front();
            }
            if (bucket < newest - 1440) { return; }
            auto it = std::lower_bound(samples.begin(), samples.end(), bucket,
                [](const Sample& s, double time) { return s.minute < time; });
            if (it == samples.end() || it->minute != bucket)
            {
                it = samples.insert(it, Sample{bucket});
            }
            it->produced += produced;
            it->consumed += consumed;
        }

        ResourceDailyRates lastDay(std::string_view resource, double minute) const
        {
            ResourceDailyRates result;
            if (!std::isfinite(minute) || minute < 0) { return result; }
            const auto it = samples_.find(std::string(resource));
            if (it == samples_.end()) { return result; }
            const double now = std::floor(minute);
            for (const auto& s : it->second)
            {
                if (s.minute > now - 1440 && s.minute <= now)
                {
                    result.production += s.produced;
                    result.depletion += s.consumed;
                }
            }
            return result;
        }
    };
} // namespace Paladin
