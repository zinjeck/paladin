#include "world/settlements/commands/SettlementCommandState.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include <algorithm>
#include <unordered_set>

namespace Paladin
{
    namespace
    {
        bool intersects(const SettlementObjectFootprint& a, const SettlementObjectFootprint& b)
        {
            return a.topLeft.x < b.topLeft.x + b.width && a.topLeft.x + a.width > b.topLeft.x
                && a.topLeft.y < b.topLeft.y + b.height && a.topLeft.y + a.height > b.topLeft.y;
        }
        std::uint64_t key(SettlementTilePosition p)
        {
            return (std::uint64_t(p.x) << 32) | std::uint32_t(p.y);
        }
    }
    bool SettlementCommandState::add(SettlementMap& map, std::string_view type,
        const SettlementObjectFootprint& area, SettlementCitizenState& citizens)
    {
        const auto* definition = SettlementCommandCatalog::definition(type);
        if (!definition || area.width <= 0 || area.height <= 0
            || !map.grid().isValidPosition(area.topLeft)
            || !map.grid().isValidPosition({area.topLeft.x + area.width - 1,
                area.topLeft.y + area.height - 1})) return false;
        pruneInvalid(map, citizens);
        std::unordered_set<std::uint64_t> existing;
        for (const auto& command : commands_)
            if (command.commandTypeId == type)
                for (const auto& target : command.targets)
                    existing.insert(key(target.footprint.topLeft));
        SettlementCommand command;
        command.commandTypeId = type;
        const auto append = [&](SettlementCommandTarget target)
        {
            if (existing.insert(key(target.footprint.topLeft)).second)
                command.targets.push_back(target);
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
        else if (definition->targetKind == CommandTargetKind::Tree
            || definition->targetKind == CommandTargetKind::Rock)
        {
            const auto expected = definition->targetKind == CommandTargetKind::Tree
                ? NaturalFeatureKind::Tree : NaturalFeatureKind::Rock;
            for (int y = area.topLeft.y; y < area.topLeft.y + area.height; ++y)
                for (int x = area.topLeft.x; x < area.topLeft.x + area.width; ++x)
                    if (map.naturalFeatures().at({x, y}).kind == expected)
                        append({{{x, y}, 1, 1}, {}, {}});
        }
        // Gather and Hunt require actual gatherable/animal entities.
        if (command.targets.empty()) return false;
        command.id = commandIds_.generate();
        for (const auto& target : command.targets)
            if (!target.objectId && !target.constructionId)
                map.naturalFeatures().mark(target.footprint.topLeft, true);
        // Designations do not reserve citizens until a work executor can claim them.
        commands_.push_back(std::move(command));
        ++version_;
        return true;
    }
    std::size_t SettlementCommandState::cancelIntersecting(SettlementMap& map,
        const SettlementObjectFootprint& area, SettlementCitizenState& citizens)
    {
        std::size_t removed = 0;
        for (auto& command : commands_)
        {
            std::erase_if(command.targets, [&](const auto& target)
            {
                if (!intersects(area, target.footprint)) return false;
                if (!target.objectId && !target.constructionId)
                    map.naturalFeatures().mark(target.footprint.topLeft, false);
                ++removed;
                return true;
            });
        }
        std::erase_if(commands_, [&](const auto& command)
        {
            if (!command.targets.empty()) return false;
            citizens.releaseCommand(command.id);
            return true;
        });
        if (removed) ++version_;
        return removed;
    }
    void SettlementCommandState::pruneInvalid(SettlementMap& map,
        SettlementCitizenState& citizens)
    {
        if (prunedObjects_ == map.objectState().presentationVersion()
            && prunedFeatures_ == map.naturalFeatures().version()) return;
        prunedObjects_ = map.objectState().presentationVersion();
        prunedFeatures_ = map.naturalFeatures().version();
        bool changed = false;
        for (auto& command : commands_)
        {
            const auto kind = SettlementCommandCatalog::definition(command.commandTypeId)->targetKind;
            const auto count = std::erase_if(command.targets, [&](const auto& target)
            {
                if (target.objectId) return !map.objectState().completedObject(target.objectId);
                if (target.constructionId) return !map.objectState().constructionSite(target.constructionId);
                const auto expected = kind == CommandTargetKind::Tree
                    ? NaturalFeatureKind::Tree : NaturalFeatureKind::Rock;
                return map.naturalFeatures().at(target.footprint.topLeft).kind != expected;
            });
            changed = changed || count > 0;
        }
        std::erase_if(commands_, [&](const auto& command)
        {
            if (!command.targets.empty()) return false;
            citizens.releaseCommand(command.id);
            return true;
        });
        if (changed) ++version_;
    }
    std::span<const SettlementCommand> SettlementCommandState::commands() const noexcept
    {
        return commands_;
    }
    std::uint64_t SettlementCommandState::version() const noexcept { return version_; }
}
