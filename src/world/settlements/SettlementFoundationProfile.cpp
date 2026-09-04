#include "world/settlements/SettlementFoundationProfile.h"

namespace Paladin
{
    SettlementFoundationProfile defaultSettlementFoundationProfile()
    {
        SettlementFoundationProfile profile;

        profile.initialPopulation = 100;
        profile.demographicRates =
            {
                0.025,
                0.015,
                0.0,
                0.20,
                0.005
            };

        profile.initialResources =
            {
                {"food", 600.0},
                {"materials", 120.0}
            };

        profile.resourceFlowRates =
            {
                {
                    "food",
                    1.05,
                    1.0,
                    1.0
                },
                {
                    "materials",
                    0.08,
                    0.02,
                    0.0
                }
            };

        profile.initialSimulationTier =
            SettlementSimulationTier::Summary;

        return profile;
    }
}
