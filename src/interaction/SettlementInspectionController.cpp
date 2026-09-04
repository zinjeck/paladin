#include "interaction/SettlementInspectionController.h"

#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

namespace Paladin
{
    bool SettlementInspectionController::selectAt(
        WorldTilePosition position,
        const SettlementObjectState& objectState,
        bool placePanelOnRight
    ) noexcept
    {
        const SettlementConstructionSite* constructionSite =
            objectState.constructionSiteAt(position);

        if (
            constructionSite &&
            constructionSite->objectTypeId != SettlementObjectTypes::Road
        )
        {
            kind_ = SettlementInspectionKind::ConstructionSite;
            constructionSiteId_ = constructionSite->id;
            objectId_ = {};
            placePanelOnRight_ = placePanelOnRight;
            return true;
        }

        const CompletedSettlementObject* object =
            objectState.completedObjectAt(position);

        if (
            object &&
            object->objectTypeId != SettlementObjectTypes::Road
        )
        {
            kind_ = SettlementInspectionKind::CompletedObject;
            objectId_ = object->id;
            constructionSiteId_ = {};
            placePanelOnRight_ = placePanelOnRight;
            return true;
        }

        clear();
        return false;
    }


    void SettlementInspectionController::clear() noexcept
    {
        kind_ = SettlementInspectionKind::None;
        objectId_ = {};
        constructionSiteId_ = {};
    }


    SettlementInspectionKind
    SettlementInspectionController::kind() const noexcept
    {
        return kind_;
    }


    bool SettlementInspectionController::placePanelOnRight() const noexcept
    {
        return placePanelOnRight_;
    }


    const CompletedSettlementObject*
    SettlementInspectionController::selectedObject(
        const SettlementObjectState& objectState
    ) const noexcept
    {
        return kind_ == SettlementInspectionKind::CompletedObject
            ? objectState.completedObject(objectId_)
            : nullptr;
    }


    const SettlementConstructionSite*
    SettlementInspectionController::selectedConstructionSite(
        const SettlementObjectState& objectState
    ) const noexcept
    {
        return kind_ == SettlementInspectionKind::ConstructionSite
            ? objectState.constructionSite(constructionSiteId_)
            : nullptr;
    }
}
