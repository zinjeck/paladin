#pragma once
#include "core/StrongId.h"

namespace Paladin
{
    // A military entity, not another citizen or an independently editable
    // headcount. The source pair identifies its ONE living person/payroll seat;
    // CitizenId alone is not unique across settlements. Family and savings
    // remain with that person while this entity travels with a world unit.
    class Soldier
    {
    public:
        Soldier(SoldierId id, SettlementId home, CitizenId person,
                SettlementObjectId barracks) noexcept
            : id_(id), home_(home), person_(person), barracks_(barracks) {}
        SoldierId id() const noexcept { return id_; }
        SettlementId homeSettlementId() const noexcept { return home_; }
        CitizenId sourceCitizenId() const noexcept { return person_; }
        SettlementObjectId barracksId() const noexcept { return barracks_; }
        ArmyId unitId() const noexcept { return unit_; }
    private:
        friend class MilitarySystem;
        SoldierId id_;
        SettlementId home_;
        CitizenId person_;
        SettlementObjectId barracks_;
        ArmyId unit_;
    };
}
