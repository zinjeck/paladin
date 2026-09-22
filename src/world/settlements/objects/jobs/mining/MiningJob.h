#pragma once

#include "world/SettlementTilePosition.h"
#include "world/WorldTile.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <algorithm>
#include <array>

namespace Paladin
{
    struct MiningJobDefinition
    {
        std::string_view type;
        std::string_view resource;
        MineralDeposit deposit;
        double minutesPerUnit;
        double maximumDepth;
        bool tunnels;
    };

    inline constexpr std::array<MiningJobDefinition, 4> MiningJobs{
        {{SettlementObjectTypes::CoalMine,
          SettlementResourceTypes::Coal,
          MineralDeposit::Coal,
          90,
          .52,
          false},
         {SettlementObjectTypes::IronMine,
          SettlementResourceTypes::Iron,
          MineralDeposit::Iron,
          120,
          .52,
          false},
         {SettlementObjectTypes::GoldMine,
          SettlementResourceTypes::Gold,
          MineralDeposit::Gold,
          3600,
          .52,
          false},
         {SettlementObjectTypes::Quarry,
          SettlementResourceTypes::Stone,
          MineralDeposit::None,
          90,
          1,
          true}}
    };

    inline int quarryEntranceCount(int width) noexcept
    {
        return std::clamp(width / 3, 1, 3);
    }

    inline int miningServiceRows(int height) noexcept
    {
        return std::min(height - 1, std::min(4, std::max(2, height - 2)));
    }

    inline SettlementTilePosition miningWorkTile(
        SettlementTilePosition origin, int width, int height,
        std::size_t ordinal) noexcept
    {
        const int firstRow = miningServiceRows(height);
        const int columns = std::max(1, width - 2);
        const int rows = std::max(1, height - firstRow);
        const auto slot = ordinal % (std::size_t(columns) * rows);
        return {origin.x + 1 + int(slot % columns),
                origin.y + firstRow + int(slot / columns)};
    }

    inline SettlementTilePosition quarryEntrance(
        SettlementTilePosition origin,
        int width,
        std::size_t ordinal,
        int height = 7
    ) noexcept
    {
        const int count = quarryEntranceCount(width);
        const int offset =
            count == 1 ? width / 2
                       : 1 + int(ordinal % count) * (width - 3) / (count - 1);
        return {origin.x + offset,
                origin.y + miningServiceRows(height)};
    }

    inline const MiningJobDefinition* miningJob(std::string_view type) noexcept
    {
        for (const auto& job : MiningJobs)
        {
            if (job.type == type)
            {
                return &job;
            }
        }
        return nullptr;
    }
} // namespace Paladin
