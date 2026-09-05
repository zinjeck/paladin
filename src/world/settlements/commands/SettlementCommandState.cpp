#include "world/settlements/commands/SettlementCommandState.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include <algorithm>

namespace Paladin
{
namespace
{
bool intersects(
    const SettlementObjectFootprint& a,
    const SettlementObjectFootprint& b
)
{
    return a.topLeft.x < b.topLeft.x + b.width &&
           a.topLeft.x + a.width > b.topLeft.x &&
           a.topLeft.y < b.topLeft.y + b.height &&
           a.topLeft.y + a.height > b.topLeft.y;
}
std::uint64_t key(SettlementTilePosition p)
{
    return (std::uint64_t(p.x) << 32) | std::uint32_t(p.y);
}
} // namespace
bool SettlementCommandState::add(
    SettlementMap& map,
    std::string_view type,
    const SettlementObjectFootprint& area,
    SettlementCitizenState& citizens
)
{
    const auto* definition = SettlementCommandCatalog::definition(type);
    if (!definition || area.width <= 0 || area.height <= 0 ||
        !map.grid().isValidPosition(area.topLeft) ||
        !map.grid().isValidPosition(
            {area.topLeft.x + area.width - 1, area.topLeft.y + area.height - 1}
        ))
        return false;
    pruneInvalid(map, citizens);
    SettlementCommand command;
    command.commandTypeId = type;
    const auto expectedTargets =
        definition->targetKind == CommandTargetKind::Object
            ? map.objectState().completedObjects().size() +
                  map.objectState().constructionSites().size()
            : map.naturalFeatures().countIn(area);
    command.targets.reserve(expectedTargets);
    command.targetIndex.reserve(expectedTargets);
    const auto append = [&](SettlementCommandTarget target)
    {
        const auto tileKey = key(target.footprint.topLeft);
        const bool exists = std::any_of(
            commands_.begin(),
            commands_.end(),
            [&](const auto& prior)
            {
                return prior.commandTypeId == type &&
                       prior.targetIndex.contains(tileKey);
            }
        );
        if (!exists)
        {
            if (command.targetIndex
                    .emplace(
                        key(target.footprint.topLeft),
                        command.targets.size()
                    )
                    .second)
                command.targets.push_back(target);
        }
    };
    if (definition->targetKind == CommandTargetKind::Object)
    {
        for (const auto& object : map.objectState().completedObjects())
            if (intersects(area, object.footprint))
                append({object.footprint, object.id, {}});
        for (const auto& site : map.objectState().constructionSites())
            if (intersects(area, site.footprint))
                append({site.footprint, {}, site.id});
    }
    else if (
        definition->targetKind == CommandTargetKind::Tree ||
        definition->targetKind == CommandTargetKind::Rock
    )
    {
        const auto expected = definition->targetKind == CommandTargetKind::Tree
                                  ? NaturalFeatureKind::Tree
                                  : NaturalFeatureKind::Rock;
        for (int y = area.topLeft.y; y < area.topLeft.y + area.height; ++y)
            for (int x = area.topLeft.x; x < area.topLeft.x + area.width; ++x)
                if (map.naturalFeatures().at({x, y}).kind == expected)
                    append({{{x, y}, 1, 1}, {}, {}});
    }
    // Gather and Hunt require actual gatherable/animal entities.
    if (command.targets.empty())
        return false;
    command.id = commandIds_.generate();
    for (const auto& target : command.targets)
        if (!target.objectId && !target.constructionId)
            map.naturalFeatures().mark(target.footprint.topLeft, true);
    // Designations do not reserve citizens until a work executor can claim
    // them.
    commands_.push_back(std::move(command));
    ++version_;
    ++selectionVersion_;
    pruning_ = false;
    return true;
}
std::size_t SettlementCommandState::cancelIntersecting(
    SettlementMap& map,
    const SettlementObjectFootprint& area,
    SettlementCitizenState& citizens
)
{
    std::size_t removed = map.objectState().cancelConstructionWithin(area);
    if (removed)
        map.logistics.synchronize(map.objectState(), 0);
    if (removed)
        map.employment().synchronize(map.objectState(), citizens);
    for (auto& command : commands_)
    {
        std::erase_if(
            command.targets,
            [&](const auto& target)
            {
                if (!intersects(area, target.footprint))
                    return false;
                if (!target.objectId && !target.constructionId)
                    map.naturalFeatures().mark(target.footprint.topLeft, false);
                ++removed;
                return true;
            }
        );
        command.targetIndex.clear();
        for (std::size_t i = 0; i < command.targets.size(); ++i)
            command.targetIndex.emplace(
                key(command.targets[i].footprint.topLeft),
                i
            );
    }
    std::erase_if(
        commands_,
        [&](const auto& command)
        {
            if (!command.targets.empty())
                return false;
            citizens.releaseCommand(command.id);
            return true;
        }
    );
    if (removed)
    {
        ++version_;
        ++selectionVersion_;
        pruning_ = false;
    }
    return removed;
}
void SettlementCommandState::pruneInvalid(
    SettlementMap& map,
    SettlementCitizenState& citizens
)
{
    if (!pruning_ && prunedObjects_ == map.objectState().navigationVersion() &&
        prunedFeatures_ == map.naturalFeatures().version())
        return;
    if (!pruning_)
    {
        prunedObjects_ = map.objectState().navigationVersion();
        prunedFeatures_ = map.naturalFeatures().version();
        pruneCommand_ = 0;
        pruneTarget_ = 0;
        pruning_ = true;
    }
    bool changed = false;
    std::size_t budget = 1024;
    while (pruneCommand_ < commands_.size() && budget > 0)
    {
        auto& command = commands_[pruneCommand_];
        const auto kind =
            SettlementCommandCatalog::definition(command.commandTypeId)
                ->targetKind;
        while (pruneTarget_ < command.targets.size() && budget > 0)
        {
            --budget;
            const auto& target = command.targets[pruneTarget_];
            const bool invalid = [&]()
            {
                if (target.objectId)
                    return !map.objectState().completedObject(target.objectId);
                if (target.constructionId)
                    return !map.objectState().constructionSite(
                        target.constructionId
                    );
                const auto expected = kind == CommandTargetKind::Tree
                                          ? NaturalFeatureKind::Tree
                                          : NaturalFeatureKind::Rock;
                return map.naturalFeatures()
                           .at(target.footprint.topLeft)
                           .kind != expected;
            }();
            if (!invalid)
            {
                ++pruneTarget_;
                continue;
            }
            command.targetIndex.erase(key(target.footprint.topLeft));
            if (pruneTarget_ + 1 < command.targets.size())
            {
                command.targets[pruneTarget_] =
                    std::move(command.targets.back());
                command.targetIndex[key(command.targets[pruneTarget_]
                                            .footprint.topLeft)] = pruneTarget_;
            }
            command.targets.pop_back();
            changed = true;
        }
        if (command.targets.empty())
        {
            citizens.releaseCommand(command.id);
            commands_.erase(commands_.begin() + pruneCommand_);
            pruneTarget_ = 0;
        }
        else if (pruneTarget_ >= command.targets.size())
        {
            ++pruneCommand_;
            pruneTarget_ = 0;
        }
    }
    if (pruneCommand_ >= commands_.size())
        pruning_ = false;
    if (changed)
        ++version_;
}
bool SettlementCommandState::contains(
    const SettlementMap& map,
    SettlementCommandId id,
    SettlementTilePosition tile,
    SettlementObjectId object,
    ConstructionSiteId site
) const
{
    for (const auto& command : commands_)
    {
        if (command.id != id)
            continue;
        const auto found = command.targetIndex.find(key(tile));
        if (found == command.targetIndex.end())
            return false;
        const auto& target = command.targets[found->second];
        if (target.objectId != object || target.constructionId != site)
            return false;
        if (object)
            return map.objectState().completedObject(object) != nullptr;
        if (site)
            return map.objectState().constructionSite(site) != nullptr;
        const auto expected =
            command.commandTypeId == SettlementCommandTypes::ChopTree
                ? NaturalFeatureKind::Tree
                : NaturalFeatureKind::Rock;
        return map.naturalFeatures().at(tile).kind == expected;
    }
    return false;
}
std::span<const SettlementCommand> SettlementCommandState::
    commands() const noexcept
{
    return commands_;
}
std::uint64_t SettlementCommandState::version() const noexcept
{
    return version_;
}
} // namespace Paladin
