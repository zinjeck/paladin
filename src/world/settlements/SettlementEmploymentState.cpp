#include "world/settlements/SettlementEmploymentState.h"
#include "world/FoundingIdentity.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/LoggingGroundsJob.h"
#include "world/settlements/objects/jobs/bakery/BakeryJob.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"
#include "world/settlements/objects/jobs/pastureland/PasturelandJob.h"
#include "world/settlements/objects/jobs/stockpile/StockpileJob.h"
#include "world/settlements/objects/jobs/trade_depot/TradeDepotJob.h"
#include "world/settlements/objects/jobs/wheat_farm/WheatFarmJob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
namespace Paladin
{
    namespace
    {
        // Reference-area capacities follow Godot; stockpile staffing is new.
        constexpr std::array<WorkplaceDefinition, 14> definitions{
            WorkplaceDefinition{SettlementObjectTypes::CoalMine, 1, 1, 9, 80},
            WorkplaceDefinition{SettlementObjectTypes::IronMine, 1, 1, 9, 80},
            WorkplaceDefinition{SettlementObjectTypes::GoldMine, 1, 1, 9, 40},
            WorkplaceDefinition{SettlementObjectTypes::Quarry, 1, 1, 9, 100},
            TradeDepotWorkplace,
            MarketWorkplace,
            StockpileWorkplace,
            FisheryWorkplace,
            WheatFarmWorkplace,
            PasturelandWorkplace,
            BakeryWorkplace,
            LoggingGroundsWorkplace,
            WorkplaceDefinition{SettlementObjectTypes::Barracks, 1, 4, 9, 100},
            WorkplaceDefinition{
                SettlementObjectTypes::ArmySupplyDepot,
                1,
                1,
                9,
                150
            }
        };
    } // namespace
    std::span<const WorkplaceDefinition> workplaceDefinitions() noexcept
    {
        return definitions;
    }
    const WorkplaceDefinition* workplaceDefinition(
        std::string_view type
    ) noexcept
    {
        for (const auto& d : definitions)
        {
            if (d.objectTypeId == type)
            {
                return &d;
            }
        }
        return nullptr;
    }
    const Workplace* SettlementEmploymentState::workplace(
        WorkplaceId id
    ) const noexcept
    {
        const auto found = std::lower_bound(
            workplaces_.begin(), workplaces_.end(), id,
            [](const Workplace& w, WorkplaceId key) { return w.id < key; });
        return found == workplaces_.end() || found->id != id ? nullptr : &*found;
    }
    Workplace* SettlementEmploymentState::mutableWorkplace(
        WorkplaceId id
    ) noexcept
    {
        return const_cast<Workplace*>(std::as_const(*this).workplace(id));
    }
    WorkplaceId SettlementEmploymentState::forObject(
        SettlementObjectId id
    ) const noexcept
    {
        const auto found = objectWorkplaces_.find(id);
        return found == objectWorkplaces_.end() ? WorkplaceId{} : found->second;
    }
    WorkplaceId SettlementEmploymentState::forConstruction(
        ConstructionSiteId id
    ) const noexcept
    {
        const auto found = constructionWorkplaces_.find(id);
        return found == constructionWorkplaces_.end() ? WorkplaceId{} : found->second;
    }
    void SettlementEmploymentState::synchronize(
        const SettlementObjectState& objects,
        SettlementCitizenState& citizens
    )
    {
        if (objectVersion_ == objects.navigationVersion())
        {
            return;
        }
        objectVersion_ = objects.navigationVersion();
        std::vector<WorkplaceId> retained;
        retained.reserve(workplaces_.size());
        const auto add = [&](std::string_view type,
                             const SettlementObjectFootprint& footprint,
                             SettlementObjectId objectId,
                             ConstructionSiteId siteId)
        {
            const auto* d = workplaceDefinition(type);
            if (!d)
            {
                return;
            }
            Workplace* found = mutableWorkplace(
                objectId ? forObject(objectId) : forConstruction(siteId));
            // A completed replacement preserves staffing/name from its exact
            // site.
            if (!found && objectId)
            {
                for (auto& w : workplaces_)
                {
                    if (w.constructionId &&
                        !objects.constructionSite(w.constructionId) &&
                        w.objectTypeId == type && w.footprint == footprint)
                    {
                        found = &w;
                        break;
                    }
                }
            }
            if (!found)
            {
                std::size_t ordinal = 1;
                for (const auto& w : workplaces_)
                {
                    if (w.objectTypeId == type)
                    {
                        ++ordinal;
                    }
                }
                const auto* objectDefinition =
                    SettlementObjectCatalog::definition(type);
                std::string name;
                do
                {
                    name = std::string(objectDefinition->displayName) + " " +
                           std::to_string(ordinal++);
                } while (std::any_of(
                    workplaces_.begin(),
                    workplaces_.end(),
                    [&](const auto& w) { return w.name == name; }
                ));
                workplaces_.push_back(
                    {ids_.generate(), {}, {}, std::string(type), name}
                );
                found = &workplaces_.back();
            }
            if (found->objectId && found->objectId != objectId)
            {
                objectWorkplaces_.erase(found->objectId);
            }
            if (found->constructionId && found->constructionId != siteId)
            {
                constructionWorkplaces_.erase(found->constructionId);
            }
            if (objectId) { objectWorkplaces_.insert_or_assign(objectId, found->id); }
            if (siteId) { constructionWorkplaces_.insert_or_assign(siteId, found->id); }
            found->objectId = objectId;
            found->constructionId = siteId;
            found->footprint = footprint;
            // Compound rooms store goods, but staff work throughout the
            // outdoor dock/yard. Staffing follows that full workplace area.
            const auto room = (type == SettlementObjectTypes::FishingGrounds ||
                               type == SettlementObjectTypes::TradeDepot)
                                  ? footprint
                                  : buildingInterior(footprint, type);
            const auto area = std::uint64_t(room.width) * room.height;
            const auto capacity =
                (area * d->workersPerReferenceArea + d->referenceArea - 1) /
                d->referenceArea;
            found->maximumCapacity = std::uint32_t(
                std::min<std::uint64_t>(
                    std::max<std::uint64_t>(d->minimumCapacity, capacity),
                    std::numeric_limits<std::uint32_t>::max()
                )
            );
            found->operational = bool(objectId);
            if (type == SettlementObjectTypes::Market)
            {
                found->maximumCapacity =
                    marketStallCount(footprint.width, footprint.height);
            }
            found->capacity =
                found->operational
                    ? std::min(found->capacity, found->maximumCapacity)
                    : 0;
            retained.push_back(found->id);
        };
        for (const auto& object : objects.completedObjects())
        {
            add(object.objectTypeId, object.footprint, object.id, {});
        }
        for (const auto& site : objects.constructionSites())
        {
            add(site.objectTypeId, site.footprint, {}, site.id);
        }
        std::sort(retained.begin(), retained.end());
        std::erase_if(
            workplaces_,
            [&](const auto& w)
            {
                if (std::binary_search(retained.begin(), retained.end(), w.id))
                {
                    return false;
                }
                if (w.objectId) { objectWorkplaces_.erase(w.objectId); }
                if (w.constructionId) { constructionWorkplaces_.erase(w.constructionId); }
                return true;
            }
        );
        std::vector<std::size_t> staff(workplaces_.size(), 0);
        for (auto& citizen : citizens.citizens_)
        {
            if (!citizen.workplaceId) { continue; }
            if (const auto* w = workplace(citizen.workplaceId))
            {
                ++staff[std::size_t(w - workplaces_.data())];
            }
            else
            {
                citizen.workplaceId = {};
                ++citizens.version_;
            }
        }
        // Preserve first-person dismissal order and protected military staff,
        // without recounting the entire population for every workplace.
        for (auto& citizen : citizens.citizens_)
        {
            if (!citizen.workplaceId || citizen.militaryUnitId || citizen.militaryDeployed)
            {
                continue;
            }
            auto* w = mutableWorkplace(citizen.workplaceId);
            auto& count = staff[std::size_t(w - workplaces_.data())];
            if (count > w->capacity)
            {
                citizen.workplaceId = {};
                citizen.nextWorkCheckMinutes = 0;
                --count;
                ++citizens.version_;
            }
        }
        for (std::size_t i = 0; i < workplaces_.size(); ++i)
        {
            workplaces_[i].capacity = std::max(workplaces_[i].capacity,
                                             std::uint32_t(staff[i]));
        }
    }
    std::size_t SettlementEmploymentState::employed(
        WorkplaceId id,
        const SettlementCitizenState& citizens
    ) const noexcept
    {
        if (!id)
        {
            return 0;
        }
        return std::count_if(
            citizens.citizens().begin(),
            citizens.citizens().end(),
            [id](const auto& c) { return c.workplaceId == id; }
        );
    }
    std::size_t SettlementEmploymentState::unemployed(
        const SettlementCitizenState& citizens
    ) const noexcept
    {
        return std::count_if(
            citizens.citizens().begin(),
            citizens.citizens().end(),
            [](const auto& c) { return !c.child && !c.militaryDeployed && c.health > 0 && !c.workplaceId; }
        );
    }
    void SettlementEmploymentState::citizenDeparted(WorkplaceId id)
    {
        if (auto* workplace = mutableWorkplace(id);
            workplace && workplace->capacity > 0)
        {
            --workplace->capacity;
        }
    }
    bool SettlementEmploymentState::adjust(
        WorkplaceId id,
        int delta,
        SettlementCitizenState& citizens
    )
    {
        auto* w = mutableWorkplace(id);
        if (!w)
        {
            return false;
        }
        if (delta == 0 || (delta > 0 && (!w->operational ||
                                         w->capacity >= w->maximumCapacity)))
        {
            return false;
        }
        for (auto& citizen : citizens.citizens_)
        {
            if (delta > 0 ? (citizen.child || citizen.militaryDeployed || citizen.health <= 0 || bool(citizen.workplaceId))
                          : (citizen.workplaceId != id || citizen.militaryUnitId ||
                             citizen.militaryDeployed))
            {
                continue;
            }
            if (delta>0 && w->objectTypeId==SettlementObjectTypes::Barracks && !citizens.militaryEligible(citizen)) continue;
            citizen.workplaceId = delta > 0 ? id : WorkplaceId{};
            if (delta > 0)
            {
                ++w->capacity;
            }
            else
            {
                w->capacity = std::min(
                    w->capacity,
                    std::uint32_t(employed(id, citizens))
                );
            }
            citizen.nextWorkCheckMinutes = 0;
            ++citizens.version_;
            return true;
        }
        return false;
    }
    bool SettlementEmploymentState::adjustType(
        std::string_view type,
        int delta,
        SettlementCitizenState& citizens
    )
    {
        const Workplace* choice = nullptr;
        std::size_t best =
            delta > 0 ? std::numeric_limits<std::size_t>::max() : 0;
        for (const auto& w : workplaces_)
        {
            if (w.objectTypeId != type || !w.operational)
            {
                continue;
            }
            const auto count = employed(w.id, citizens);
            if (delta > 0 ? (w.capacity < w.maximumCapacity && count < best)
                          : (count > best))
            {
                choice = &w;
                best = count;
            }
        }
        return choice && adjust(choice->id, delta, citizens);
    }
    bool SettlementEmploymentState::rename(
        WorkplaceId id,
        std::string_view name
    )
    {
        const std::string trimmed = trimFoundingName(name);
        if (!isValidFoundingName(trimmed))
        {
            return false;
        }
        if (auto* w = mutableWorkplace(id))
        {
            w->name = trimmed;
            return true;
        }
        return false;
    }
    void SettlementEmploymentState::record(
        double minute,
        const SettlementCitizenState& citizens
    )
    {
        std::size_t adults = 0, withoutWork = 0;
        for (const auto& c : citizens.citizens())
        {
            if (!c.child && !c.militaryDeployed && c.health > 0)
            {
                ++adults;
                withoutWork += !c.workplaceId;
            }
        }
        const double percent =
            adults ? 100.0 * withoutWork / adults : 0;
        if (!history_.empty())
        {
            if (minute < history_.back().gameMinute)
            {
                return;
            }
            if (minute == history_.back().gameMinute)
            {
                history_.back().unemployedPercent = percent;
                return;
            }
            if (minute - history_.back().gameMinute < 60 &&
                history_.back().unemployedPercent == percent)
            {
                return;
            }
        }
        history_.push_back({minute, percent});
        const double oldest = minute - 16 * 1440;
        while (history_.size() > 2 && history_[1].gameMinute < oldest)
        {
            history_.pop_front();
        }
    }
} // namespace Paladin
