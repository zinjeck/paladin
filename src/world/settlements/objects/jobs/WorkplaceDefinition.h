#pragma once
#include <cstdint>
#include <span>
#include <string_view>
namespace Paladin
{
struct WorkplaceDefinition
{
    std::string_view objectTypeId;
    std::uint32_t minimumCapacity;
    std::uint32_t workersPerReferenceArea;
    std::uint32_t referenceArea;
    int storageCapacity = 50;
};
std::span<const WorkplaceDefinition> workplaceDefinitions() noexcept;
const WorkplaceDefinition* workplaceDefinition(std::string_view type) noexcept;
} // namespace Paladin
