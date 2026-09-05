#include "simulation/systems/SettlementJobBoard.h"
#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
namespace Paladin
{
    bool SettlementJobBoard::claimed(const CitizenTask& task) const
    {
        if (task.kind == CitizenTaskKind::Gather)
        {
            return gatheringClaims_.contains(
                (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
                std::uint32_t(task.workTile.x)
            );
        }
        if (task.kind == CitizenTaskKind::Demolish)
        {
            return task.object ? demolitionClaims_.contains(task.object)
                               : siteDemolitionClaims_.contains(task.site);
        }
        return false;
    }
    void SettlementJobBoard::claim(const CitizenTask& task)
    {
        if (task.kind == CitizenTaskKind::Gather)
        {
            gatheringClaims_.insert(
                (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
                std::uint32_t(task.workTile.x)
            );
        }
        else if (task.kind == CitizenTaskKind::Demolish)
        {
            if (task.object)
            {
                demolitionClaims_.insert(task.object);
            }
            else if (task.site)
            {
                siteDemolitionClaims_.insert(task.site);
            }
        }
    }
    void SettlementJobBoard::releaseClaim(const CitizenTask& task)
    {
        if (task.kind == CitizenTaskKind::Gather)
        {
            gatheringClaims_.erase(
                (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
                std::uint32_t(task.workTile.x)
            );
        }
        else if (task.kind == CitizenTaskKind::Demolish)
        {
            if (task.object)
            {
                demolitionClaims_.erase(task.object);
            }
            else if (task.site)
            {
                siteDemolitionClaims_.erase(task.site);
            }
        }
    }
    void SettlementJobBoard::refreshConstructionBoard(const SettlementMap& map)
    {
        const auto version = map.objectState().navigationVersion();
        if (boardVersion_ == version)
        {
            return;
        }
        constructionBuckets_.clear();
        for (const auto& site : map.objectState().constructionSites())
        {
            const auto& f = site.footprint;
            for (int y = f.topLeft.y / 32;
                 y <= (f.topLeft.y + f.height - 1) / 32;
                 ++y)
            {
                for (int x = f.topLeft.x / 32;
                     x <= (f.topLeft.x + f.width - 1) / 32;
                     ++x)
                {
                    constructionBuckets_
                        [(std::uint64_t(y) << 32) | std::uint32_t(x)]
                            .push_back(site.id);
                }
            }
        }
        std::erase_if(
            clearingCursors_,
            [&](const auto& entry)
            { return !map.objectState().constructionSite(entry.first); }
        );
        boardVersion_ = version;
    }
    CitizenTask SettlementJobBoard::CommandWork::task() const
    {
        CitizenTask result;
        result.kind = object || site ? CitizenTaskKind::Demolish
                                     : CitizenTaskKind::Gather;
        result.command = command;
        result.object = object;
        result.site = site;
        result.workTile = footprint.topLeft;
        return result;
    }
    bool SettlementJobBoard::refreshCommandBoard(const SettlementMap& map)
    {
        const auto& state = map.commandState();
        if (commandBoardVersion_ != state.selectionVersion() ||
            (!commandBoardReady_ && commandSourceVersion_ != state.version()))
        {
            commandJobs_.clear();
            commandBuckets_.clear();
            availableCommandJobs_.clear();
            commandBuildCommand_ = commandBuildTarget_ = commandSweep_ = 0;
            commandBoardReady_ = false;
            commandBoardVersion_ = state.selectionVersion();
            commandSourceVersion_ = state.version();
        }
        // Build and retire opportunities incrementally. A map-wide designation
        // must never become a map-wide burst of AI work on the following frame.
        std::size_t budget = 2048;
        const auto commands = state.commands();
        while (!commandBoardReady_ && commandBuildCommand_ < commands.size() &&
               budget > 0)
        {
            const auto& command = commands[commandBuildCommand_];
            while (commandBuildTarget_ < command.targets.size() && budget > 0)
            {
                --budget;
                const auto& target = command.targets[commandBuildTarget_++];
                const auto p = target.footprint.topLeft;
                auto& bucket = commandBuckets_
                    [(std::uint64_t(p.y / 32) << 32) | std::uint32_t(p.x / 32)];
                const auto index = commandJobs_.size();
                commandJobs_.push_back(
                    {command.id,
                     target.objectId,
                     target.constructionId,
                     target.footprint,
                     availableCommandJobs_.size(),
                     bucket.size()}
                );
                bucket.push_back(index);
                availableCommandJobs_.push_back(index);
            }
            if (commandBuildTarget_ >= command.targets.size())
            {
                ++commandBuildCommand_;
                commandBuildTarget_ = 0;
            }
        }
        if (commandBuildCommand_ >= commands.size())
        {
            commandBoardReady_ = true;
        }
        budget = std::min<std::size_t>(1024, availableCommandJobs_.size());
        while (commandBoardReady_ && !availableCommandJobs_.empty() &&
               budget-- > 0)
        {
            commandSweep_ %= availableCommandJobs_.size();
            const auto index = availableCommandJobs_[commandSweep_];
            auto& job = commandJobs_[index];
            if (state.contains(
                    map,
                    job.command,
                    job.footprint.topLeft,
                    job.object,
                    job.site
                ))
            {
                ++commandSweep_;
                continue;
            }
            const auto p = job.footprint.topLeft;
            auto& bucket = commandBuckets_
                [(std::uint64_t(p.y / 32) << 32) | std::uint32_t(p.x / 32)];
            bucket[job.bucketSlot] = bucket.back();
            commandJobs_[bucket.back()].bucketSlot = job.bucketSlot;
            bucket.pop_back();
            availableCommandJobs_[job.globalSlot] =
                availableCommandJobs_.back();
            commandJobs_[availableCommandJobs_.back()].globalSlot =
                job.globalSlot;
            availableCommandJobs_.pop_back();
            job.available = false;
        }
        return commandBoardReady_;
    }

    void SettlementJobBoard::resetClaims()
    {
        gatheringClaims_.clear();
        demolitionClaims_.clear();
        siteDemolitionClaims_.clear();
    }
    std::span<const ConstructionSiteId> SettlementJobBoard::constructionBucket(
        std::uint64_t key
    ) const
    {
        const auto found = constructionBuckets_.find(key);
        return found == constructionBuckets_.end()
                   ? std::span<const ConstructionSiteId>{}
                   : found->second;
    }
    std::span<const std::size_t> SettlementJobBoard::commandBucket(
        std::uint64_t key
    ) const
    {
        const auto found = commandBuckets_.find(key);
        return found == commandBuckets_.end() ? std::span<const std::size_t>{}
                                              : found->second;
    }
    std::optional<SettlementTilePosition> SettlementJobBoard::
        nextClearingTarget(
            const SettlementMap& map,
            ConstructionSiteId id,
            const SettlementObjectFootprint& footprint,
            std::size_t& budget
        )
    {
        return map.naturalFeatures()
            .nextIn(footprint, clearingCursors_[id], budget);
    }
} // namespace Paladin
