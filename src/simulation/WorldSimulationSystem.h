#pragma once

namespace Paladin
{
    class World;

    class WorldSimulationSystem
    {
    public:
        virtual ~WorldSimulationSystem() = default;

        virtual void tick(
            World& world,
            double gameDeltaSeconds
        ) = 0;
    };
}
