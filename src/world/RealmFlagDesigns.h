#pragma once

#include "world/FoundingIdentity.h"
#include <array>
#include <cstdlib>

namespace Paladin
{
    // Small, editable heraldry: twelve silhouettes and twelve coordinated
    // palettes. A realm ordinal chooses a distinct design without an RNG.
    inline constexpr std::size_t RealmFlagDesignCount = 144;

    inline RealmFlag realmFlagDesign(std::size_t ordinal)
    {
        constexpr std::array<MapColor, 12> fields{
            {{166, 53, 69},
             {63, 95, 154},
             {35, 87, 71},
             {111, 74, 126},
             {57, 70, 88},
             {183, 106, 54},
             {79, 140, 122},
             {99, 62, 75},
             {51, 122, 88},
             {48, 69, 93},
             {135, 77, 80},
             {116, 81, 63}}
        };
        constexpr MapColor gold{235, 196, 107};
        constexpr MapColor ivory{244, 243, 232};
        constexpr MapColor dark{8, 15, 27};
        RealmFlag flag;
        const auto field = fields[(ordinal / 12) % fields.size()];
        const auto accent = (ordinal / 12) % 2 ? ivory : gold;
        flag.primaryColor = accent;
        for (int y = 0; y < 9; ++y)
        {
            for (int x = 0; x < 7; ++x)
            {
                bool charge = false;
                switch (ordinal % 12)
                {
                case 0:
                    charge = x == 2 || y == 3;
                    break;
                case 1:
                    charge = std::abs(x - 3) + std::abs(y - 4) <= 2;
                    break;
                case 2:
                    charge = y == 2 || y == 6;
                    break;
                case 3:
                    charge = x == 1 || x == 5;
                    break;
                case 4:
                    charge = std::abs(x - 3) == std::abs(y - 4);
                    break;
                case 5:
                    charge = y == std::abs(x - 3) + 3;
                    break;
                case 6:
                    charge = (x + y) % 4 < 2;
                    break;
                case 7:
                    charge = x == 3 || (y == 3 && x > 0 && x < 6);
                    break;
                case 8:
                    charge = (x < 3) != (y < 4);
                    break;
                case 9:
                    charge = y >= 3 && y <= 5 && x >= 2 && x <= 4;
                    break;
                case 10:
                    charge = y == 4 || (y == 3 && x % 2 == 1);
                    break;
                case 11:
                    charge = x == 3 || y == 4 || (x % 2 && y % 4 == 0);
                    break;
                }
                auto color = charge ? accent : field;
                if (ordinal % 12 == 9 && x == 3 && y == 4)
                {
                    color = dark;
                }
                flag.cells[std::size_t(y * 7 + x)] = {true, color};
            }
        }
        return flag;
    }
} // namespace Paladin
