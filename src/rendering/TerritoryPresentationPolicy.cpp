#include "rendering/TerritoryPresentationPolicy.h"

namespace Paladin
{
    const TerritoryPresentationPolicy&
    defaultTerritoryPresentationPolicy() noexcept
    {
        static const TerritoryPresentationPolicy policy;
        return policy;
    }
}
