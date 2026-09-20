#pragma once

#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/objects/jobs/mining/MiningJob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
    struct MiningSiteProgress
    {
        double labor = 0;
        double productionCredit = 0;
        std::size_t scanned = 0, cursor = 0;
        std::vector<SettlementTilePosition> columns;
        bool exhausted = false;
    };

    // Separate from citizen activity: physical deposits survive demolition,
    // whereas staffing, reservations and job choice use the ordinary systems.
    class SettlementMiningState
    {
    public:
        void synchronize(const SettlementObjectState& objects)
        {
            if (revision_ == objects.navigationVersion())
            {
                return;
            }
            revision_ = objects.navigationVersion();
            std::erase_if(
                sites_,
                [&](const auto& site)
                { return !objects.completedObject(site.first); }
            );
        }
        const MiningSiteProgress* find(SettlementObjectId id) const
        {
            const auto it = sites_.find(id);
            return it == sites_.end() ? nullptr : &it->second;
        }

        MiningSiteProgress& prepare(
            const SettlementGrid& grid,
            const CompletedSettlementObject& object
        )
        {
            auto& site = sites_[object.id];
            const auto* job = miningJob(object.objectTypeId);
            if (!job)
            {
                return site;
            }
            const auto& f = object.footprint;
            const auto area = std::size_t(f.width) * f.height;
            // Discovery is cursor-based even for a map-sized selection.
            const auto end = std::min(area, site.scanned + 512);
            for (; site.scanned < end; ++site.scanned)
            {
                SettlementTilePosition p{
                    f.topLeft.x + int(site.scanned % f.width),
                    f.topLeft.y + int(site.scanned / f.width)
                };
                const auto* tile = grid.tile(p);
                if (tile && tile->terrain == TerrainType::Land &&
                    (job->deposit == MineralDeposit::None ||
                     tile->mineral == job->deposit))
                {
                    site.columns.push_back(p);
                }
            }
            site.exhausted =
                site.scanned == area && site.cursor == site.columns.size();
            return site;
        }

        double depth(const CompletedSettlementObject& object) const
        {
            const auto* job = miningJob(object.objectTypeId);
            const auto* site = find(object.id);
            if (!job || !site)
            {
                return 0;
            }
            const double area =
                double(object.footprint.width) * object.footprint.height;
            return job->maximumDepth *
                   std::clamp(site->labor / (area * 720), 0., 1.);
        }

        int work(
            const SettlementGrid& grid,
            const CompletedSettlementObject& object,
            int workers,
            double elapsed,
            int room
        )
        {
            const auto* job = miningJob(object.objectTypeId);
            if (!job || workers <= 0 || room <= 0 || !std::isfinite(elapsed) ||
                elapsed <= 0)
            {
                return 0;
            }
            auto& site = prepare(grid, object);
            if (site.exhausted || site.cursor >= site.columns.size())
            {
                return 0;
            }
            const double labor =
                std::min(elapsed * workers, (room + 1) * job->minutesPerUnit);
            site.labor += labor;
            site.productionCredit = std::min(
                double(room),
                site.productionCredit + labor / job->minutesPerUnit
            );
            int wanted = std::min(room, int(site.productionCredit));
            const int yield = job->deposit == MineralDeposit::Gold   ? 8
                              : job->deposit == MineralDeposit::Iron ? 250
                              : job->deposit == MineralDeposit::Coal ? 400
                                                                     : 10000;
            int produced = 0;
            int inspected = 0;
            while (wanted > 0 && site.cursor < site.columns.size() &&
                   inspected++ < 256)
            {
                const auto p = site.columns[site.cursor];
                const auto key = std::uint64_t(std::uint32_t(p.y)) << 32 |
                                 std::uint32_t(p.x);
                auto& removed = extracted_[key][std::size_t(job->deposit)];
                const int take = std::min(wanted, std::max(0, yield - removed));
                removed += take;
                produced += take;
                wanted -= take;
                if (removed >= yield)
                {
                    ++site.cursor;
                }
            }
            site.productionCredit -= produced;
            site.exhausted =
                site.scanned == std::size_t(object.footprint.width) *
                                    object.footprint.height &&
                site.cursor == site.columns.size();
            return produced;
        }

    private:
        std::uint64_t revision_ = ~std::uint64_t(0);
        std::unordered_map<SettlementObjectId, MiningSiteProgress, StrongIdHash>
            sites_;
        std::unordered_map<std::uint64_t, std::array<int, 4>> extracted_;
    };
} // namespace Paladin
