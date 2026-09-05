#pragma once
#include "core/StrongId.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <deque>
#include <span>
#include <string>
#include <vector>
namespace Paladin
{
class SettlementMap;
class SettlementCitizenState;
struct Workplace
{
    WorkplaceId id;
    SettlementObjectId objectId;
    ConstructionSiteId constructionId;
    std::string objectTypeId;
    std::string name;
    SettlementObjectFootprint footprint;
    // Player-controlled staffing slots; unopened workplaces start at 0/0.
    std::uint32_t capacity = 0;
    std::uint32_t maximumCapacity = 0;
    bool operational = false;
};
struct UnemploymentSample
{
    double gameMinute = 0;
    double unemployedPercent = 100;
};
class SettlementEmploymentState
{
  public:
    void synchronize(const SettlementObjectState&, SettlementCitizenState&);
    std::span<const Workplace> workplaces() const noexcept
    {
        return workplaces_;
    }
    const Workplace* workplace(WorkplaceId) const noexcept;
    WorkplaceId forObject(SettlementObjectId) const noexcept;
    WorkplaceId forConstruction(ConstructionSiteId) const noexcept;
    std::size_t employed(
        WorkplaceId,
        const SettlementCitizenState&
    ) const noexcept;
    std::size_t unemployed(const SettlementCitizenState&) const noexcept;
    bool adjust(WorkplaceId, int delta, SettlementCitizenState&);
    void citizenDeparted(WorkplaceId);
    bool adjustType(std::string_view, int delta, SettlementCitizenState&);
    bool rename(WorkplaceId, std::string_view);
    void record(double minute, const SettlementCitizenState&);
    const std::deque<UnemploymentSample>& history() const noexcept
    {
        return history_;
    }

  private:
    std::vector<Workplace> workplaces_;
    IdGenerator<WorkplaceId> ids_;
    std::uint64_t objectVersion_ = ~std::uint64_t(0);
    std::deque<UnemploymentSample> history_;
};
} // namespace Paladin
