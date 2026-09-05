#include "world/settlements/citizens/SettlementCitizenState.h"

#include "world/TerrainType.h"
#include "world/SettlementGrid.h"
#include "world/WorldTile.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <cstddef>
#include <deque>
#include <string_view>

namespace Paladin
{
    namespace
    {
        constexpr std::array<std::string_view, 100> maleNames{
            "Arlen", "Tovan", "Calen", "Ronan", "Darian",
            "Kael", "Bren", "Orin", "Levon", "Theron",
            "Jarek", "Corin", "Malric", "Edrin", "Tomas",
            "Varon", "Lucan", "Alric", "Fenric", "Soren",
            "Aldren", "Beran", "Cedran", "Doran", "Evren",
            "Garric", "Hadren", "Ivarn", "Joren", "Kellan",
            "Merek", "Nolan", "Odran", "Perric", "Roder",
            "Stellan", "Torren", "Ulren", "Wystan", "Yorick",
            "Adrian", "Aldric", "Ansel", "Arden", "Asher",
            "Bastian", "Benedict", "Bram", "Caelan", "Cassian",
            "Caspar", "Cillian", "Conrad", "Damon", "Darius",
            "Declan", "Dominic", "Edric", "Edwin", "Elias",
            "Emrys", "Ewan", "Fabian", "Felix", "Finn",
            "Florian", "Gareth", "Gavin", "Gideon", "Godric",
            "Harlan", "Hector", "Hugo", "Idris", "Jasper",
            "Julian", "Kendrick", "Laurence", "Leander", "Lionel",
            "Lorcan", "Magnus", "Merrick", "Nathan", "Osric",
            "Owen", "Percival", "Quentin", "Raphael", "Rhys",
            "Roland", "Rowan", "Silas", "Simeon", "Tobias",
            "Tristan", "Valentin", "Victor", "Walter", "Wilfred"
        };

        constexpr std::array<std::string_view, 100> femaleNames{
            "Mira", "Elia", "Sera", "Nira", "Liora",
            "Kaela", "Maris", "Elara", "Vessa", "Talia",
            "Rina", "Anya", "Selene", "Maera", "Isolde",
            "Lyra", "Vela", "Seris", "Amara", "Coralie",
            "Aveline", "Briala", "Ceryn", "Delara", "Eirwen",
            "Fiora", "Giselle", "Halia", "Ilara", "Jessamine",
            "Kerra", "Lenora", "Mirelle", "Nerissa", "Odelle",
            "Petra", "Roselyn", "Sabine", "Thalia", "Ysara",
            "Adela", "Adelaide", "Adrienne", "Agnes", "Ailsa",
            "Alina", "Annora", "Arabella", "Astrid", "Aurelia",
            "Beatrice", "Branwen", "Brielle", "Camilla", "Carina",
            "Cassandra", "Cecilia", "Celeste", "Clara", "Cordelia",
            "Della", "Dorothea", "Edith", "Eleanor", "Elise",
            "Elowen", "Emilia", "Enid", "Estelle", "Eva",
            "Freya", "Genevieve", "Guinevere", "Helena", "Imogen",
            "Ingrid", "Iona", "Iris", "Johanna", "Judith",
            "Lavinia", "Leona", "Linnea", "Livia", "Lucille",
            "Lydia", "Margot", "Matilda", "Melisande", "Minerva",
            "Nadia", "Noelle", "Ophelia", "Oriana", "Philippa",
            "Rosalind", "Rowena", "Theodora", "Viola", "Winifred"
        };

        bool isWalkableCitizenTile(
            const SettlementMap& settlementMap,
            SettlementTilePosition position
        ) noexcept
        {
            const SettlementGrid& grid = settlementMap.grid();
            const WorldTile* tile = grid.tile(position);

            if (
                !tile ||
                tile->terrain == TerrainType::Water ||
                tile->terrain == TerrainType::Mountain
            )
            {
                return false;
            }

            const SettlementObjectState& objectState =
                settlementMap.objectState();
            const CompletedSettlementObject* object =
                objectState.completedObjectAt(position);
            const SettlementConstructionSite* constructionSite =
                objectState.constructionSiteAt(position);

            const auto blocksCitizen = [](std::string_view objectTypeId)
            {
                const SettlementObjectDefinition* definition =
                    SettlementObjectCatalog::definition(objectTypeId);
                return
                    !definition ||
                    definition->placementLayer ==
                        SettlementObjectPlacementLayer::Structure;
            };

            return
                (!object || !blocksCitizen(object->objectTypeId)) &&
                (
                    !constructionSite ||
                    !blocksCitizen(constructionSite->objectTypeId)
                );
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

        behaviorSeed_ = nameSeed;
        return spawn(citizenCount);
    }

    bool SettlementCitizenState::spawn(std::uint64_t citizenCount)
    {
        if (!citizenCount || citizenCount > 100000 ||
            citizens_.size() > citizens_.max_size() - citizenCount)
        {
            return false;
        }
        const auto nameSeed = behaviorSeed_;
        const auto first = citizens_.size();
        citizens_.reserve(first + static_cast<std::size_t>(citizenCount));

        const auto savedIds = citizenIds_;
        try
        {
            for (std::uint64_t index = first; index < first + citizenCount;
                 ++index)
            {
                const CitizenSex sex =
                    (GenerationNoise::mix(nameSeed ^ (index * 104729ULL)) &
                     1U) == 0
                        ? CitizenSex::Male
                        : CitizenSex::Female;
                const auto& names =
                    sex == CitizenSex::Male ? maleNames : femaleNames;
                const std::size_t poolIndex = static_cast<std::size_t>(
                    (nameSeed + index * 17U) % names.size()
                );

                citizens_.push_back(
                    {citizenIds_.generate(), std::string(names[poolIndex]), sex}
                );
            }
        }
        catch (...)
        {
            citizens_.resize(first);
            citizenIds_ = savedIds;
            throw;
        }
        ++version_;
        return true;
    }

    void SettlementCitizenState::placeUnpositionedCitizens(
        const SettlementMap& settlementMap
    )
    {
        const SettlementGrid& grid = settlementMap.grid();
        if (grid.width() <= 0 || grid.height() <= 0)
        {
            return;
        }

        const CompletedSettlementObject* cityKeep = nullptr;

        for (const CompletedSettlementObject& object :
            settlementMap.objectState().completedObjects())
        {
            if (object.objectTypeId == SettlementObjectTypes::CityKeep)
            {
                cityKeep = &object;
                break;
            }
        }

        if (!cityKeep)
        {
            return;
        }

        std::vector<SettlementTilePosition> spawnTiles;
        spawnTiles.reserve(citizens_.size());
        std::vector<std::uint8_t> visited(
            static_cast<std::size_t>(grid.width()) *
                static_cast<std::size_t>(grid.height()),
            0
        );
        std::deque<SettlementTilePosition> searchFrontier;

        const auto enqueue = [&grid, &visited, &searchFrontier](
            SettlementTilePosition position
        )
        {
            if (!grid.isValidPosition(position))
            {
                return;
            }

            const std::size_t index =
                static_cast<std::size_t>(position.y) *
                    static_cast<std::size_t>(grid.width()) +
                static_cast<std::size_t>(position.x);

            if (visited[index] != 0)
            {
                return;
            }

            visited[index] = 1;
            searchFrontier.push_back(position);
        };

        for (
            std::int32_t y = cityKeep->footprint.topLeft.y;
            y < cityKeep->footprint.topLeft.y + cityKeep->footprint.height;
            ++y
        )
        {
            for (
                std::int32_t x = cityKeep->footprint.topLeft.x;
                x < cityKeep->footprint.topLeft.x + cityKeep->footprint.width;
                ++x
            )
            {
                enqueue({x, y});
            }
        }

        constexpr std::array<SettlementTilePosition, 4> neighborOffsets{{
            {0, -1}, {-1, 0}, {1, 0}, {0, 1}
        }};

        while (
            !searchFrontier.empty() &&
            spawnTiles.size() < citizens_.size()
        )
        {
            const SettlementTilePosition position = searchFrontier.front();
            searchFrontier.pop_front();

            if (
                !cityKeep->footprint.contains(position) &&
                isWalkableCitizenTile(settlementMap, position)
            )
            {
                spawnTiles.push_back(position);
            }

            for (const SettlementTilePosition offset : neighborOffsets)
            {
                enqueue({
                    position.x + offset.x,
                    position.y + offset.y
                });
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
            citizen.path.clear();
            citizen.stepProgress = 0;
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
            citizen.idleWait = -1;
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


    const SettlementCitizen* SettlementCitizenState::citizen(
        CitizenId id
    ) const noexcept
    {
        const auto iterator = std::find_if(
            citizens_.begin(),
            citizens_.end(),
            [id](const SettlementCitizen& citizen)
            {
                return citizen.id == id;
            }
        );

        return iterator == citizens_.end() ? nullptr : &*iterator;
    }


    const SettlementCitizen* SettlementCitizenState::citizenAt(
        SettlementTilePosition position
    ) const noexcept
    {
        const auto iterator = std::find_if(
            citizens_.rbegin(),
            citizens_.rend(),
            [position](const SettlementCitizen& citizen)
            {
                return SettlementTilePosition{int(std::floor(citizen.visualX() + .5)),
                    int(std::floor(citizen.visualY() + .5))} == position;
            }
        );

        return iterator == citizens_.rend() ? nullptr : &*iterator;
    }


    std::uint64_t SettlementCitizenState::version() const noexcept
    {
        return version_;
    }
}
