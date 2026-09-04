#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

namespace Paladin
{
    class SettlementObjectState;
    struct CompletedSettlementObject;
    struct SettlementConstructionSite;

    enum class SettlementInspectionKind
    {
        None,
        CompletedObject,
        ConstructionSite
    };

    class SettlementInspectionController
    {
    public:
        [[nodiscard]]
        bool selectAt(
            WorldTilePosition position,
            const SettlementObjectState& objectState,
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

    private:
        SettlementInspectionKind kind_ = SettlementInspectionKind::None;
        SettlementObjectId objectId_;
        ConstructionSiteId constructionSiteId_;
        bool placePanelOnRight_ = true;
    };
}
