#include "world/World.h"

namespace Paladin
{
    World::World() = default;

    World::~World() = default;

    void World::tick(double deltaSeconds)
    {
        // Strategic world simulation will eventually advance here.
        //
        // Examples:
        // - settlements progress
        // - armies move
        // - polity AI evaluates decisions
        // - diplomacy changes
        // - world-scale systems update
        //
        // Rendering must never be required for this function to work.

        (void)deltaSeconds;
    }
}