#include "world/settlements/commands/SettlementCommandState.h"

#include "world/WorldGrid.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"

#include <algorithm>

namespace Paladin
{
    namespace
    {
        bool intersects(
            const SettlementObjectFootprint& first,
            const SettlementObjectFootprint& second
        ) noexcept
        {
            return
                first.topLeft.x < second.topLeft.x + second.width &&
                first.topLeft.x + first.width > second.topLeft.x &&
                first.topLeft.y < second.topLeft.y + second.height &&
                first.topLeft.y + first.height > second.topLeft.y;
        }
    }


    bool SettlementCommandState::add(
        const WorldGrid& grid,
        std::string_view commandTypeId,
        const SettlementObjectFootprint& footprint,
        SettlementCitizenState& citizens
    )
    {
        const SettlementCommandDefinition* definition =
            SettlementCommandCatalog::definition(commandTypeId);
        const WorldTilePosition bottomRight{
            footprint.topLeft.x + footprint.width - 1,
            footprint.topLeft.y + footprint.height - 1
        };

        if (
            !definition || footprint.width <= 0 || footprint.height <= 0 ||
            !grid.isValidPosition(footprint.topLeft) ||
            !grid.isValidPosition(bottomRight)
        )
        {
            return false;
        }

        const SettlementCommandId id = commandIds_.generate();
        const CitizenId assignedCitizen = definition->assignsCitizen
            ? citizens.assignIdleCitizen(id)
            : CitizenId{};

        commands_.push_back({
            id,
            std::string(commandTypeId),
            footprint,
            assignedCitizen
        });
        ++version_;
        return true;
    }


    std::size_t SettlementCommandState::cancelIntersecting(
        const SettlementObjectFootprint& footprint,
        SettlementCitizenState& citizens
    )
    {
        std::size_t removed = 0;
        std::erase_if(
            commands_,
            [&footprint, &citizens, &removed](
                const SettlementCommand& command
            )
            {
                if (!intersects(command.footprint, footprint))
                {
                    return false;
                }

                citizens.releaseCommand(command.id);
                ++removed;
                return true;
            }
        );

        if (removed > 0)
        {
            ++version_;
        }

        return removed;
    }


    std::span<const SettlementCommand>
    SettlementCommandState::commands() const noexcept
    {
        return commands_;
    }


    std::uint64_t SettlementCommandState::version() const noexcept
    {
        return version_;
    }
}
