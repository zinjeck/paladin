#pragma once

#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"

namespace Paladin
{
    class SettlementObjectState;
    class SettlementCitizenState;
    struct CompletedSettlementObject;
    struct SettlementConstructionSite;
    struct SettlementCitizen;

    enum class SettlementInspectionKind
    {
        None,
        CompletedObject,
        ConstructionSite,
        Citizen
    };

    class SettlementInspectionController
    {
    public:
        [[nodiscard]]
        bool selectAt(
            SettlementTilePosition position,
            const SettlementObjectState& objectState,
            const SettlementCitizenState& citizenState,
            bool placePanelOnRight
        ) noexcept;

        void clear() noexcept;

        [[nodiscard]]
        SettlementInspectionKind kind() const noexcept;

        [[nodiscard]]
        bool placePanelOnRight() const noexcept;

        [[nodiscard]]
        const CompletedSettlementObject* selectedObject(
            const SettlementObjectState& objectState
        ) const noexcept;

        [[nodiscard]]
        const SettlementConstructionSite* selectedConstructionSite(
            const SettlementObjectState& objectState
        ) const noexcept;

        [[nodiscard]]
        const SettlementCitizen* selectedCitizen(
            const SettlementCitizenState& citizenState
        ) const noexcept;

    private:
        SettlementInspectionKind kind_ = SettlementInspectionKind::None;
        SettlementObjectId objectId_;
        ConstructionSiteId constructionSiteId_;
        CitizenId citizenId_;
        bool placePanelOnRight_ = true;
    };
}
