#include "world/settlements/citizens/SettlementCitizenState.h"

#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/settlements/SettlementMap.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace Paladin
{
    namespace
    {
        constexpr std::array<std::string_view, 40> maleNames{
            "Arlen", "Tovan", "Calen", "Ronan", "Darian", "Kael",
            "Bren", "Orin", "Levon", "Theron", "Jarek", "Corin",
            "Malric", "Edrin", "Tomas", "Varon", "Lucan", "Alric",
            "Fenric", "Soren", "Aldren", "Beran", "Cedran", "Doran",
            "Evren", "Garric", "Hadren", "Ivarn", "Joren", "Kellan",
            "Merek", "Nolan", "Odran", "Perric", "Roder", "Stellan",
            "Torren", "Ulren", "Wystan", "Yorick"
        };

        constexpr std::array<std::string_view, 40> femaleNames{
            "Mira", "Elia", "Sera", "Nira", "Liora", "Kaela", "Maris",
            "Elara", "Vessa", "Talia", "Rina", "Anya", "Selene",
            "Maera", "Isolde", "Lyra", "Vela", "Seris", "Amara",
            "Coralie", "Aveline", "Briala", "Ceryn", "Delara", "Eirwen",
            "Fiora", "Giselle", "Halia", "Ilara", "Jessamine", "Kerra",
            "Lenora", "Mirelle", "Nerissa", "Odelle", "Petra", "Roselyn",
            "Sabine", "Thalia", "Ysara"
        };

        bool isWalkableCitizenTile(
            const WorldGrid& grid,
            WorldTilePosition position
        ) noexcept
        {
            const WorldTile* tile = grid.tile(position);
            return
                tile &&
                tile->terrain != TerrainType::Water &&
                tile->terrain != TerrainType::Mountain;
        }
    }


    bool SettlementCitizenState::initialize(
        std::uint64_t citizenCount,
        std::uint64_t nameSeed
    )
    {
        if (!citizens_.empty() || citizenCount == 0)
        {
            return false;
        }

        citizens_.reserve(static_cast<std::size_t>(citizenCount));
        const std::uint64_t maleCount = citizenCount / 2;

        for (std::uint64_t index = 0; index < citizenCount; ++index)
        {
            const CitizenSex sex = index < maleCount
                ? CitizenSex::Male
                : CitizenSex::Female;
            const auto& names = sex == CitizenSex::Male
                ? maleNames
                : femaleNames;
            const std::size_t poolIndex = static_cast<std::size_t>(
                (nameSeed + index * 17U) % names.size()
            );

            citizens_.push_back({
                citizenIds_.generate(),
                std::string(names[poolIndex]),
                sex
            });
        }

        ++version_;
        return true;
    }


    void SettlementCitizenState::placeUnpositionedCitizens(
        const SettlementMap& settlementMap
    )
    {
        const WorldGrid& grid = settlementMap.grid();
        if (grid.width() <= 0 || grid.height() <= 0)
        {
            return;
        }

        const WorldTilePosition center{
            grid.width() / 2,
            grid.height() / 2
        };
        std::vector<WorldTilePosition> spawnTiles;
        spawnTiles.reserve(citizens_.size());

        for (
            std::int32_t radius = 0;
            spawnTiles.size() < citizens_.size() &&
                radius < grid.width() + grid.height();
            ++radius
        )
        {
            for (std::int32_t y = center.y - radius; y <= center.y + radius; ++y)
            {
                for (std::int32_t x = center.x - radius; x <= center.x + radius; ++x)
                {
                    if (
                        radius > 0 &&
                        x > center.x - radius && x < center.x + radius &&
                        y > center.y - radius && y < center.y + radius
                    )
                    {
                        continue;
                    }

                    const WorldTilePosition position{x, y};
                    if (isWalkableCitizenTile(grid, position))
                    {
                        spawnTiles.push_back(position);
                    }
                }
            }
        }

        if (spawnTiles.empty())
        {
            return;
        }

        bool changed = false;
        std::size_t spawnIndex = 0;
        for (SettlementCitizen& citizen : citizens_)
        {
            if (grid.isValidPosition(citizen.tilePosition))
            {
                continue;
            }

            citizen.tilePosition =
                spawnTiles[spawnIndex % spawnTiles.size()];
            ++spawnIndex;
            changed = true;
        }

        if (changed)
        {
            ++version_;
        }
    }


    CitizenId SettlementCitizenState::assignIdleCitizen(
        SettlementCommandId commandId
    ) noexcept
    {
        if (!commandId.isValid())
        {
            return {};
        }

        for (SettlementCitizen& citizen : citizens_)
        {
            if (citizen.activity != CitizenActivity::Idle)
            {
                continue;
            }

            citizen.activity = CitizenActivity::AssignedToCommand;
            citizen.assignedCommandId = commandId;
            ++version_;
            return citizen.id;
        }

        return {};
    }


    void SettlementCitizenState::releaseCommand(
        SettlementCommandId commandId
    ) noexcept
    {
        bool changed = false;
        for (SettlementCitizen& citizen : citizens_)
        {
            if (citizen.assignedCommandId != commandId)
            {
                continue;
            }

            citizen.activity = CitizenActivity::Idle;
            citizen.assignedCommandId = {};
            changed = true;
        }

        if (changed)
        {
            ++version_;
        }
    }


    std::span<const SettlementCitizen>
    SettlementCitizenState::citizens() const noexcept
    {
        return citizens_;
    }


    std::uint64_t SettlementCitizenState::version() const noexcept
    {
        return version_;
    }
}
