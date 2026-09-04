#include "interaction/SettlementInspectionController.h"

#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

namespace Paladin
{
    bool SettlementInspectionController::selectAt(
        SettlementTilePosition position,
        const SettlementObjectState& objectState,
        const SettlementCitizenState& citizenState,
        bool placePanelOnRight
    ) noexcept
    {
        const SettlementCitizen* citizen =
            citizenState.citizenAt(position);

        if (citizen)
        {
            kind_ = SettlementInspectionKind::Citizen;
            citizenId_ = citizen->id;
            constructionSiteId_ = {};
            objectId_ = {};
            placePanelOnRight_ = placePanelOnRight;
            return true;
        }

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
            citizenId_ = {};
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
            citizenId_ = {};
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
        citizenId_ = {};
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


    const SettlementCitizen*
    SettlementInspectionController::selectedCitizen(
        const SettlementCitizenState& citizenState
    ) const noexcept
    {
        return kind_ == SettlementInspectionKind::Citizen
            ? citizenState.citizen(citizenId_)
            : nullptr;
    }
}
