#include "world/settlements/SettlementEmploymentState.h"
#include "world/FoundingIdentity.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/bakery/BakeryJob.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/pastureland/PasturelandJob.h"
#include "world/settlements/objects/jobs/stockpile/StockpileJob.h"
#include "world/settlements/objects/jobs/wheat_farm/WheatFarmJob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace Paladin
{
namespace
{
// Reference-area capacities follow Godot; stockpile staffing is new.
constexpr std::array<WorkplaceDefinition, 5> definitions{
    StockpileWorkplace,
    FisheryWorkplace,
    WheatFarmWorkplace,
    PasturelandWorkplace,
    BakeryWorkplace
};
} // namespace
std::span<const WorkplaceDefinition> workplaceDefinitions() noexcept
{
    return definitions;
}
const WorkplaceDefinition* workplaceDefinition(std::string_view type) noexcept
{
    for (const auto& d : definitions)
        if (d.objectTypeId == type)
            return &d;
    return nullptr;
}
const Workplace* SettlementEmploymentState::workplace(
    WorkplaceId id
) const noexcept
{
    for (const auto& w : workplaces_)
        if (w.id == id)
            return &w;
    return nullptr;
}
WorkplaceId SettlementEmploymentState::forObject(
    SettlementObjectId id
) const noexcept
{
    for (const auto& w : workplaces_)
        if (id && w.objectId == id)
            return w.id;
    return {};
}
WorkplaceId SettlementEmploymentState::forConstruction(
    ConstructionSiteId id
) const noexcept
{
    for (const auto& w : workplaces_)
        if (id && w.constructionId == id)
            return w.id;
    return {};
}
void SettlementEmploymentState::synchronize(
    const SettlementObjectState& objects,
    SettlementCitizenState& citizens
)
{
    if (objectVersion_ == objects.navigationVersion())
        return;
    objectVersion_ = objects.navigationVersion();
    std::vector<WorkplaceId> retained;
    const auto add = [&](std::string_view type,
                         const SettlementObjectFootprint& footprint,
                         SettlementObjectId objectId,
                         ConstructionSiteId siteId)
    {
        const auto* d = workplaceDefinition(type);
        if (!d)
            return;
        Workplace* found = nullptr;
        for (auto& w : workplaces_)
            if ((objectId && w.objectId == objectId) ||
                (siteId && w.constructionId == siteId))
            {
                found = &w;
                break;
            }
        // A completed replacement preserves staffing/name from its exact site.
        if (!found && objectId)
            for (auto& w : workplaces_)
                if (w.constructionId &&
                    !objects.constructionSite(w.constructionId) &&
                    w.objectTypeId == type && w.footprint == footprint)
                {
                    found = &w;
                    break;
                }
        if (!found)
        {
            std::size_t ordinal = 1;
            for (const auto& w : workplaces_)
                if (w.objectTypeId == type)
                    ++ordinal;
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
        found->objectId = objectId;
        found->constructionId = siteId;
        found->footprint = footprint;
        const auto area = std::uint64_t(footprint.width) * footprint.height;
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
        found->capacity =
            found->operational
                ? std::min(found->capacity, found->maximumCapacity)
                : 0;
        retained.push_back(found->id);
    };
    for (const auto& object : objects.completedObjects())
        add(object.objectTypeId, object.footprint, object.id, {});
    for (const auto& site : objects.constructionSites())
        add(site.objectTypeId, site.footprint, {}, site.id);
    std::erase_if(
        workplaces_,
        [&](const auto& w)
        {
            return std::find(retained.begin(), retained.end(), w.id) ==
                   retained.end();
        }
    );
    for (auto& citizen : citizens.citizens_)
    {
        if (citizen.workplaceId && !workplace(citizen.workplaceId))
        {
            citizen.workplaceId = {};
            ++citizens.version_;
        }
    }
    for (const auto& w : workplaces_)
        while (employed(w.id, citizens) > w.capacity)
            adjust(w.id, -1, citizens);
}
std::size_t SettlementEmploymentState::employed(
    WorkplaceId id,
    const SettlementCitizenState& citizens
) const noexcept
{
    if (!id)
        return 0;
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
        [](const auto& c) { return !c.workplaceId; }
    );
}
void SettlementEmploymentState::citizenDeparted(WorkplaceId id)
{
    for (auto& workplace : workplaces_)
    {
        if (workplace.id == id && workplace.capacity > 0)
        {
            --workplace.capacity;
            return;
        }
    }
}
bool SettlementEmploymentState::adjust(
    WorkplaceId id,
    int delta,
    SettlementCitizenState& citizens
)
{
    auto found = std::find_if(
        workplaces_.begin(),
        workplaces_.end(),
        [id](const auto& w) { return w.id == id; }
    );
    if (found == workplaces_.end())
        return false;
    auto* w = &*found;
    if (delta == 0 ||
        (delta > 0 && (!w->operational || w->capacity >= w->maximumCapacity)))
        return false;
    for (auto& citizen : citizens.citizens_)
    {
        if (delta > 0 ? bool(citizen.workplaceId) : citizen.workplaceId != id)
            continue;
        citizen.workplaceId = delta > 0 ? id : WorkplaceId{};
        if (delta > 0)
            ++w->capacity;
        else
            w->capacity =
                std::min(w->capacity, std::uint32_t(employed(id, citizens)));
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
    std::size_t best = delta > 0 ? std::numeric_limits<std::size_t>::max() : 0;
    for (const auto& w : workplaces_)
    {
        if (w.objectTypeId != type || !w.operational)
            continue;
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
bool SettlementEmploymentState::rename(WorkplaceId id, std::string_view name)
{
    const std::string trimmed = trimFoundingName(name);
    if (!isValidFoundingName(trimmed))
        return false;
    for (auto& w : workplaces_)
        if (w.id == id)
        {
            w.name = trimmed;
            return true;
        }
    return false;
}
void SettlementEmploymentState::record(
    double minute,
    const SettlementCitizenState& citizens
)
{
    const double percent =
        citizens.citizens().empty()
            ? 0
            : 100.0 * unemployed(citizens) / citizens.citizens().size();
    if (!history_.empty())
    {
        if (minute < history_.back().gameMinute)
            return;
        if (minute == history_.back().gameMinute)
        {
            history_.back().unemployedPercent = percent;
            return;
        }
        if (minute - history_.back().gameMinute < 60 &&
            history_.back().unemployedPercent == percent)
            return;
    }
    history_.push_back({minute, percent});
    const double oldest = minute - 16 * 1440;
    while (history_.size() > 2 && history_[1].gameMinute < oldest)
        history_.pop_front();
}
} // namespace Paladin
