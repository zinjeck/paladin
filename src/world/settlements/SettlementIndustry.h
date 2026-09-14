#pragma once
#include "core/StrongId.h"
namespace Paladin {
    class SettlementMap;
    bool produceIndustry(SettlementMap&, SettlementObjectId, int workers,
                         double minute, double elapsed);
}
