#pragma once

namespace Paladin
{
    // Detail describes simulation cost, not ownership. The current player
    // settlement is detailed, background player settlements use summaries,
    // and distant or AI settlements use strategic batches.
    enum class SettlementSimulationTier
    {
        Detailed,
        Summary,
        Strategic
    };
}
