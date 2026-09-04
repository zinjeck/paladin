#include "world/settlements/SettlementEmploymentState.h"
#include "world/FoundingIdentity.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
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
    {{SettlementObjectTypes::Stockpile, 2, 2, 4},
     {SettlementObjectTypes::FishingGrounds, 4, 4, 9},
     {SettlementObjectTypes::WheatFarm, 1, 1, 25},
     {SettlementObjectTypes::Pastureland, 1, 1, 12},
     {SettlementObjectTypes::Bakery, 1, 1, 6}}
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
    if (objectVersion_ == objects.presentationVersion())
        return;
    objectVersion_ = objects.presentationVersion();
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
            if (citizen.activity == CitizenActivity::TravelingToWork ||
                citizen.activity == CitizenActivity::AtWork)
            {
                citizen.activity = CitizenActivity::Idle;
                citizen.path.clear();
                citizen.stepProgress = 0;
                citizen.idleWait = -1;
            }
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
        if (citizen.activity != CitizenActivity::AssignedToCommand)
        {
            citizen.path.clear();
            citizen.pathIndex = 0;
            citizen.stepProgress = 0;
            citizen.activity = CitizenActivity::Idle;
            citizen.idleWait = -1;
        }
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
void SettlementEmploymentState::tickAttendance(
    const SettlementMap& map,
    SettlementCitizenState& citizens,
    double minute
)
{
    if (citizens.citizens_.empty() || !std::isfinite(minute) ||
        !std::isfinite(behaviorPolicy.retryMinutes) ||
        behaviorPolicy.retryMinutes <= 0)
        return;
    const int clock = int(std::fmod(minute, 1440));
    const bool onShift = clock >= behaviorPolicy.shiftStartMinute &&
                         clock < behaviorPolicy.shiftEndMinute;
    std::size_t paths = 0;
    for (std::size_t scan = 0;
         scan < std::min(
                    behaviorPolicy.attendanceChecksPerTick,
                    citizens.citizens_.size()
                );
         ++scan)
    {
        auto& c =
            citizens.citizens_[attendanceCursor_++ % citizens.citizens_.size()];
        const auto* w = workplace(c.workplaceId);
        if (!w || !map.grid().isValidPosition(c.tilePosition) ||
            c.activity == CitizenActivity::AssignedToCommand)
            continue;
        if (!onShift || !w->operational)
        {
            if (c.activity == CitizenActivity::AtWork ||
                c.activity == CitizenActivity::TravelingToWork)
            {
                c.activity = CitizenActivity::Idle;
                c.path.clear();
                c.stepProgress = 0;
                c.idleWait = -1;
                ++citizens.version_;
            }
            continue;
        }
        if (c.activity == CitizenActivity::TravelingToWork && c.path.empty())
        {
            c.activity = c.tilePosition == c.destination
                             ? CitizenActivity::AtWork
                             : CitizenActivity::Idle;
            ++citizens.version_;
        }
        if (c.activity == CitizenActivity::AtWork || !c.path.empty() ||
            minute < c.nextWorkCheckMinutes)
            continue;
        if (paths >= behaviorPolicy.pathRequestsPerTick)
            break;
        ++paths;
        c.nextWorkCheckMinutes = minute + behaviorPolicy.retryMinutes;
        const auto& f = w->footprint;
        // Sample the closest edge, not every tile of an arbitrarily large
        // workplace.
        const std::array<SettlementTilePosition, 4> access{
            {{std::clamp(
                  c.tilePosition.x,
                  f.topLeft.x,
                  f.topLeft.x + f.width - 1
              ),
              f.topLeft.y - 1},
             {std::clamp(
                  c.tilePosition.x,
                  f.topLeft.x,
                  f.topLeft.x + f.width - 1
              ),
              f.topLeft.y + f.height},
             {f.topLeft.x - 1,
              std::clamp(
                  c.tilePosition.y,
                  f.topLeft.y,
                  f.topLeft.y + f.height - 1
              )},
             {f.topLeft.x + f.width,
              std::clamp(
                  c.tilePosition.y,
                  f.topLeft.y,
                  f.topLeft.y + f.height - 1
              )}}
        };
        const auto goal = access
            [(std::uint64_t(minute / behaviorPolicy.retryMinutes) +
              c.id.value()) %
             access.size()];
        if (citizens.moveTo(c.id, map, goal))
        {
            c.activity = c.tilePosition == goal
                             ? CitizenActivity::AtWork
                             : CitizenActivity::TravelingToWork;
            ++citizens.version_;
        }
    }
}
} // namespace Paladin
