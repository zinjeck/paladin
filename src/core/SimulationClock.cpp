#include "core/SimulationClock.h"

#include <SDL3/SDL.h>

#include <cmath>

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
        speedMultiplier_ = 1.0;
        paused_ = true;
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
        if (!paused_)
        {
            accumulatorSeconds_ +=
                frameTimeSeconds * speedMultiplier_;
        }
    }

    void SimulationClock::setPaused(bool paused) noexcept
    {
        paused_ = paused;
    }

    void SimulationClock::setSpeedMultiplier(
        double multiplier
    ) noexcept
    {
        if (std::isfinite(multiplier) && multiplier > 0.0)
        {
            speedMultiplier_ = multiplier;
        }
    }

    bool SimulationClock::shouldTick() const
    {
        return
            !paused_ &&
            accumulatorSeconds_ >= fixedDeltaSeconds_;
    }

    void SimulationClock::consumeTick()
    {
        accumulatorSeconds_ -= fixedDeltaSeconds_;
    }

    bool SimulationClock::isPaused() const noexcept
    {
        return paused_;
    }

    double SimulationClock::speedMultiplier() const noexcept
    {
        return speedMultiplier_;
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
