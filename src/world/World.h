#pragma once

namespace Paladin
{
    class World
    {
    public:
        World();
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        void tick(double deltaSeconds);

    private:
        // World is the authoritative owner of strategic game state.
        //
        // Future state owned here will include things such as:
        //
        // - settlements
        // - polities
        // - armies
        // - diplomacy
        // - strategic movement
        // - world geography
        // - world-scale economic relationships
        //
        // Detailed settlement state will remain owned by each
        // individual settlement rather than becoming global state.
    };
}