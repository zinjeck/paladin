#pragma once
#include <cstdint>

namespace Paladin
{
    class SimulationClock
    {
    public:
        explicit SimulationClock(double ticksPerSecond);

        void reset() noexcept;
        void beginFrame();

        void setPaused(bool paused) noexcept;
        void setSpeedMultiplier(double multiplier) noexcept;

        bool shouldTick() const;
        double backlogTicks() const noexcept
        {
            return accumulatorSeconds_ / fixedDeltaSeconds_;
        }
        std::uint64_t limitHits = 0;
        double discardedSeconds = 0;
        void consumeTick();

        bool isPaused() const noexcept;
        double speedMultiplier() const noexcept;
        double fixedDeltaSeconds() const noexcept;
        double frameDeltaSeconds() const noexcept;
        double interpolationAlpha() const noexcept;

    private:
        double fixedDeltaSeconds_ = 0.05;
        double frameDeltaSeconds_ = 0.0;
        double accumulatorSeconds_ = 0.0;
        double previousTimeSeconds_ = 0.0;
        double speedMultiplier_ = 1.0;
        bool paused_ = true;
        bool firstFrame_ = true;
    };
}
