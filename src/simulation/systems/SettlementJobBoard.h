#pragma once
#include "world/settlements/objects/SettlementObjectState.h"
#include <deque>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
namespace Paladin
{
    class SettlementMap;
    struct CitizenTask;
    // Owns opportunity indexing, incremental maintenance and exclusive claims.
    // Activity selection queries this board; it does not maintain its caches.
    class SettlementJobBoard
    {
    public:
        struct CommandWork
        {
            SettlementCommandId command;
            SettlementObjectId object;
            ConstructionSiteId site;
            SettlementObjectFootprint footprint;
            std::size_t globalSlot = 0;
            std::size_t bucketSlot = 0;
            bool available = true;
            CitizenTask task() const;
        };
        bool claimed(const CitizenTask&) const;
        void claim(const CitizenTask&);
        void releaseClaim(const CitizenTask&);
        void resetClaims();
        bool refreshCommandBoard(const SettlementMap&);
        void refreshConstructionBoard(const SettlementMap&);
        std::span<const ConstructionSiteId> constructionBucket(
            std::uint64_t key
        ) const;
        std::span<const std::size_t> commandBucket(std::uint64_t key) const;
        std::span<const std::size_t> availableCommands() const
        {
            return availableCommandJobs_;
        }
        const CommandWork& command(std::size_t index) const
        {
            return commandJobs_[index];
        }
        std::optional<SettlementTilePosition> nextClearingTarget(
            const SettlementMap&,
            ConstructionSiteId,
            const SettlementObjectFootprint&,
            std::size_t& budget
        );

    private:
        std::unordered_set<std::uint64_t> gatheringClaims_;
        std::unordered_set<SettlementObjectId, StrongIdHash> demolitionClaims_;
        std::unordered_set<ConstructionSiteId, StrongIdHash>
            siteDemolitionClaims_;
        std::uint64_t commandBoardVersion_ = ~std::uint64_t(0);
        std::uint64_t commandSourceVersion_ = 0;
        std::size_t commandBuildCommand_ = 0;
        std::size_t commandBuildTarget_ = 0;
        std::size_t commandSweep_ = 0;
        bool commandBoardReady_ = false;
        std::deque<CommandWork> commandJobs_;
        std::vector<std::size_t> availableCommandJobs_;
        std::unordered_map<std::uint64_t, std::vector<std::size_t>>
            commandBuckets_;
        std::uint64_t boardVersion_ = ~std::uint64_t(0);
        std::unordered_map<std::uint64_t, std::vector<ConstructionSiteId>>
            constructionBuckets_;
        std::unordered_map<ConstructionSiteId, std::size_t, StrongIdHash>
            clearingCursors_;
    };
} // namespace Paladin
