#pragma once

#include "world/generation/WorldGenerationSettings.h"

#include <cstdint>

namespace Paladin
{
    [[nodiscard]]
    std::uint64_t nextRandomWorldSeed();

    [[nodiscard]]
    WorldGenerationSettings withRandomWorldSeed(
        WorldGenerationSettings settings = {}
    );
}
