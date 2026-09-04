#pragma once

namespace Paladin
{
    class SimulationClock
    {
    public:
        explicit SimulationClock(double ticksPerSecond);

        void beginFrame();

        bool shouldTick() const;
        void consumeTick();

        double fixedDeltaSeconds() const noexcept;
        double frameDeltaSeconds() const noexcept;
        double interpolationAlpha() const noexcept;

    private:
        double fixedDeltaSeconds_ = 0.05;
        double frameDeltaSeconds_ = 0.0;
        double accumulatorSeconds_ = 0.0;
        double previousTimeSeconds_ = 0.0;
        bool firstFrame_ = true;
    };
}
