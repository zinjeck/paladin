#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementResourceDefinition.h"

namespace Paladin
{
    SettlementFoundationProfile playerSettlementFoundationProfile(std::uint64_t seed)
    {
        auto profile = defaultSettlementFoundationProfile();
        profile.initialPopulation = profile.initialDetailedCitizenCount;
        profile.citizenSeed = seed;
        // Player goods will come from concrete tasks/production, not passive
        // per-resident placeholder flows.
        profile.resourceFlowRates.clear();
        // Aggregate births/deaths would invent residents without citizen records.
        // A later lifecycle system will create/remove both together.
        profile.demographicRates = {0, 0, 0, 0, 0};
        return profile;
    }

    SettlementFoundationProfile defaultSettlementFoundationProfile()
    {
        SettlementFoundationProfile profile;

        profile.initialPopulation = 100;
        profile.initialDetailedCitizenCount = 8;
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
                {std::string(SettlementResourceTypes::Food), 600.0},
                {std::string(SettlementResourceTypes::Materials), 120.0},
                {std::string(SettlementResourceTypes::Stone), 0.0},
                {std::string(SettlementResourceTypes::Lumber), 0.0}
            };

        profile.resourceFlowRates =
            {
                {
                    std::string(SettlementResourceTypes::Food),
                    1.05,
                    1.0,
                    1.0
                },
                {
                    std::string(SettlementResourceTypes::Materials),
                    0.08,
                    0.02,
                    0.0
                }
            };

        profile.initialSimulationTier =
            SettlementSimulationTier::Inactive;

        return profile;
    }
}
