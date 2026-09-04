#pragma once
#include "core/StrongId.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <deque>
#include <span>
#include <string>
#include <vector>
namespace Paladin
{
class SettlementMap;
class SettlementCitizenState;
struct WorkplaceDefinition
{
    std::string_view objectTypeId;
    std::uint32_t minimumCapacity;
    std::uint32_t workersPerReferenceArea;
    std::uint32_t referenceArea;
};
std::span<const WorkplaceDefinition> workplaceDefinitions() noexcept;
const WorkplaceDefinition* workplaceDefinition(std::string_view type) noexcept;
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
struct WorkplaceBehaviorPolicy
{
    int shiftStartMinute = 8 * 60;
    int shiftEndMinute = 17 * 60;
    double retryMinutes = 5;
    std::size_t attendanceChecksPerTick = 32;
    std::size_t pathRequestsPerTick = 1;
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
    bool adjustType(std::string_view, int delta, SettlementCitizenState&);
    bool rename(WorkplaceId, std::string_view);
    void record(double minute, const SettlementCitizenState&);
    const std::deque<UnemploymentSample>& history() const noexcept
    {
        return history_;
    }
    void tickAttendance(
        const SettlementMap&,
        SettlementCitizenState&,
        double minute
    );
    WorkplaceBehaviorPolicy behaviorPolicy;

  private:
    std::vector<Workplace> workplaces_;
    IdGenerator<WorkplaceId> ids_;
    std::uint64_t objectVersion_ = ~std::uint64_t(0);
    std::deque<UnemploymentSample> history_;
    std::size_t attendanceCursor_ = 0;
};
} // namespace Paladin
