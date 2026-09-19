#pragma once

#include "core/StrongId.h"

namespace Paladin
{
    // Only deployment coordinates are local. Personnel always remain in World.
    struct BattleSoldierView
    {
        SoldierId soldier;
        double x = 0, y = 0;
        bool player = false, female = false;
    };
} // namespace Paladin
