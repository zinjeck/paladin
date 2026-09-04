#include "world/territory/TerritoryFoundationPolicy.h"

namespace Paladin
{
    const TerritoryTerrainRule&
    TerritoryFoundationPolicy::ruleFor(
        TerrainType terrain
    ) const noexcept
    {
        switch (terrain)
        {
            case TerrainType::Land:
                return land;

            case TerrainType::Water:
                return water;

            case TerrainType::Mountain:
                return mountain;
        }

        return water;
    }


    const TerritoryFoundationPolicy&
    defaultTerritoryFoundationPolicy() noexcept
    {
        static const TerritoryFoundationPolicy policy;
        return policy;
    }
}
