#pragma once
#include <cstdint>

namespace Paladin
{
    enum class GovernanceType : std::uint8_t
    { AbsoluteMonarchy, LimitedMonarchy, ConstitutionalMonarchy, ParliamentaryMonarchy };
    enum class CitizenshipRights : std::uint8_t { Blood, AcceptedCulture, FullCitizenship };
    enum class GenderRights : std::uint8_t { MaleDominated, Equal, FemaleDominated };

    struct RealmLaws
    {
        GovernanceType governance = GovernanceType::AbsoluteMonarchy;
        CitizenshipRights citizenship = CitizenshipRights::Blood;
        GenderRights gender = GenderRights::MaleDominated;
        // The same qualification applies to military service and future
        // governor, noble and commander appointment systems.
        [[nodiscard]] bool allowsMilitaryOrOffice(bool female) const noexcept
        {
            return gender == GenderRights::Equal ||
                (gender == GenderRights::FemaleDominated ? female : !female);
        }
        bool operator==(const RealmLaws&) const = default;
    };
}
