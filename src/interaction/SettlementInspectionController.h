#pragma once

#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"
#include "world/settlements/SettlementLogistics.h"

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
        Citizen,
        Groundpile
    };

    class SettlementInspectionController
    {
    public:
      [[nodiscard]]
      bool selectAt(
          SettlementTilePosition position,
          const SettlementObjectState& objectState,
          const SettlementCitizenState& citizenState,
          bool placePanelOnRight,
          const SettlementLogistics* logistics = nullptr
      ) noexcept;

      const SettlementInventory* selectedInventory(
          const SettlementLogistics& logistics
      ) const
      {
          return kind_ == SettlementInspectionKind::Groundpile
                     ? logistics.inventory(inventoryId_)
                     : nullptr;
      }
        void clear() noexcept;
        void selectWorkplace(SettlementObjectId object, ConstructionSiteId site) noexcept
        {
            clear();
            objectId_ = object;
            constructionSiteId_ = site;
            kind_ = object ? SettlementInspectionKind::CompletedObject
                : site ? SettlementInspectionKind::ConstructionSite : SettlementInspectionKind::None;
            placePanelOnRight_ = true;
        }

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
        InventoryId inventoryId_;
        bool placePanelOnRight_ = true;
    };
}
