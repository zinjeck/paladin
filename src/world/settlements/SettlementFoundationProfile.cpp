#include "world/settlements/SettlementFoundationProfile.h"

namespace Paladin
{
    SettlementFoundationProfile defaultSettlementFoundationProfile()
    {
        return {
            100,
            {
                0.025,
                0.015,
                0.0
            },
            {
                {"food", 600.0},
                {"materials", 120.0}
            }
        };
    }
}
