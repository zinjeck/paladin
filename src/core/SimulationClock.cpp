#include "core/SimulationClock.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    SimulationClock::SimulationClock(double ticksPerSecond)
        : fixedDeltaSeconds_(1.0 / ticksPerSecond)
    {
    }

    void SimulationClock::reset() noexcept
    {
        frameDeltaSeconds_ = 0.0;
        accumulatorSeconds_ = 0.0;
        previousTimeSeconds_ = 0.0;
        firstFrame_ = true;
    }

    void SimulationClock::beginFrame()
    {
        frameDeltaSeconds_ = 0.0;

        const double currentTimeSeconds =
            static_cast<double>(SDL_GetTicksNS()) / 1'000'000'000.0;

        if (firstFrame_)
        {
            previousTimeSeconds_ = currentTimeSeconds;
            firstFrame_ = false;
            return;
        }

        double frameTimeSeconds =
            currentTimeSeconds - previousTimeSeconds_;

        previousTimeSeconds_ = currentTimeSeconds;

        // Prevent the simulation from trying to catch up forever
        // after a breakpoint, window drag, or temporary stall.
        constexpr double maxFrameTimeSeconds = 0.25;

        if (frameTimeSeconds > maxFrameTimeSeconds)
        {
            frameTimeSeconds = maxFrameTimeSeconds;
        }

        frameDeltaSeconds_ = frameTimeSeconds;
        accumulatorSeconds_ += frameTimeSeconds;
    }

    bool SimulationClock::shouldTick() const
    {
        return accumulatorSeconds_ >= fixedDeltaSeconds_;
    }

    void SimulationClock::consumeTick()
    {
        accumulatorSeconds_ -= fixedDeltaSeconds_;
    }

    double SimulationClock::fixedDeltaSeconds() const noexcept
    {
        return fixedDeltaSeconds_;
    }

    double SimulationClock::frameDeltaSeconds() const noexcept
    {
        return frameDeltaSeconds_;
    }

    double SimulationClock::interpolationAlpha() const noexcept
    {
        return accumulatorSeconds_ / fixedDeltaSeconds_;
    }
}
