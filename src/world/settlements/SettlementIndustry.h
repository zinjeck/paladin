#pragma once
#include "core/StrongId.h"
#include <string_view>
namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    bool isIndustry(std::string_view type) noexcept;
    // One-to-one wheat -> bread and ordinary food -> rations. The barracks
    // purchases already-produced rations from completed supply depots only.
    bool produceIndustry(SettlementMap&, SettlementObjectId, int workers,
                         double minute, double elapsed);
    void advanceInactiveIndustry(SettlementMap&, const SettlementCitizenState&, double minute, double elapsed);
}
