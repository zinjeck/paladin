#include "world/settlements/citizens/SettlementCitizenState.h"

#include "world/SettlementGrid.h"
#include "world/TerrainType.h"
#include "world/WorldTile.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <string_view>
#include <utility>

namespace Paladin
{
    namespace
    {
        constexpr std::array<std::string_view, 100> maleNames{
            "Arlen",   "Tovan",    "Calen",    "Ronan",   "Darian",
            "Kael",    "Bren",     "Orin",     "Levon",   "Theron",
            "Jarek",   "Corin",    "Malric",   "Edrin",   "Tomas",
            "Varon",   "Lucan",    "Alric",    "Fenric",  "Soren",
            "Aldren",  "Beran",    "Cedran",   "Doran",   "Evren",
            "Garric",  "Hadren",   "Ivarn",    "Joren",   "Kellan",
            "Merek",   "Nolan",    "Odran",    "Perric",  "Roder",
            "Stellan", "Torren",   "Ulren",    "Wystan",  "Yorick",
            "Adrian",  "Aldric",   "Ansel",    "Arden",   "Asher",
            "Bastian", "Benedict", "Bram",     "Caelan",  "Cassian",
            "Caspar",  "Cillian",  "Conrad",   "Damon",   "Darius",
            "Declan",  "Dominic",  "Edric",    "Edwin",   "Elias",
            "Emrys",   "Ewan",     "Fabian",   "Felix",   "Finn",
            "Florian", "Gareth",   "Gavin",    "Gideon",  "Godric",
            "Harlan",  "Hector",   "Hugo",     "Idris",   "Jasper",
            "Julian",  "Kendrick", "Laurence", "Leander", "Lionel",
            "Lorcan",  "Magnus",   "Merrick",  "Nathan",  "Osric",
            "Owen",    "Percival", "Quentin",  "Raphael", "Rhys",
            "Roland",  "Rowan",    "Silas",    "Simeon",  "Tobias",
            "Tristan", "Valentin", "Victor",   "Walter",  "Wilfred"
        };

        constexpr std::array<std::string_view, 100> femaleNames{
            "Mira",      "Elia",      "Sera",      "Nira",      "Liora",
            "Kaela",     "Maris",     "Elara",     "Vessa",     "Talia",
            "Rina",      "Anya",      "Selene",    "Maera",     "Isolde",
            "Lyra",      "Vela",      "Seris",     "Amara",     "Coralie",
            "Aveline",   "Briala",    "Ceryn",     "Delara",    "Eirwen",
            "Fiora",     "Giselle",   "Halia",     "Ilara",     "Jessamine",
            "Kerra",     "Lenora",    "Mirelle",   "Nerissa",   "Odelle",
            "Petra",     "Roselyn",   "Sabine",    "Thalia",    "Ysara",
            "Adela",     "Adelaide",  "Adrienne",  "Agnes",     "Ailsa",
            "Alina",     "Annora",    "Arabella",  "Astrid",    "Aurelia",
            "Beatrice",  "Branwen",   "Brielle",   "Camilla",   "Carina",
            "Cassandra", "Cecilia",   "Celeste",   "Clara",     "Cordelia",
            "Della",     "Dorothea",  "Edith",     "Eleanor",   "Elise",
            "Elowen",    "Emilia",    "Enid",      "Estelle",   "Eva",
            "Freya",     "Genevieve", "Guinevere", "Helena",    "Imogen",
            "Ingrid",    "Iona",      "Iris",      "Johanna",   "Judith",
            "Lavinia",   "Leona",     "Linnea",    "Livia",     "Lucille",
            "Lydia",     "Margot",    "Matilda",   "Melisande", "Minerva",
            "Nadia",     "Noelle",    "Ophelia",   "Oriana",    "Philippa",
            "Rosalind",  "Rowena",    "Theodora",  "Viola",     "Winifred"
        };

        bool isWalkableCitizenTile(
            const SettlementMap& settlementMap,
            SettlementTilePosition position
        ) noexcept
        {
            const SettlementGrid& grid = settlementMap.grid();
            const WorldTile* tile = grid.tile(position);

            if (!tile || tile->terrain == TerrainType::Water ||
                tile->terrain == TerrainType::Mountain)
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
                return !definition ||
                       definition->placementLayer ==
                           SettlementObjectPlacementLayer::Structure;
            };

            return (!object || !blocksCitizen(object->objectTypeId)) &&
                   (!constructionSite ||
                    !blocksCitizen(constructionSite->objectTypeId));
        }
    } // namespace


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
        if (!appendCitizens(citizenCount, false))
        {
            return false;
        }
        // The founding party is always four men and four women, in seeded
        // order.
        if (citizenCount == 8)
        {
            const auto offset = GenerationNoise::mix(nameSeed) % 8;
            for (std::size_t i = 0; i < 8; ++i)
            {
                auto& c = citizens_[i];
                c.sex = ((i + offset) % 8 < 4) ? CitizenSex::Male
                                               : CitizenSex::Female;
                c.name =
                    (c.sex == CitizenSex::Male
                         ? maleNames
                         : femaleNames)[(nameSeed + i * 17) % maleNames.size()];
            }
        }
        matchSingles();
        return true;
    }

    std::string_view SettlementCitizenState::maleName(
        std::uint64_t index
    ) noexcept
    {
        return maleNames[index % maleNames.size()];
    }

    bool SettlementCitizenState::chooseFoundingRulerName(std::string_view name)
    {
        if (std::find(maleNames.begin(), maleNames.end(), name) ==
            maleNames.end())
        {
            return false;
        }
        for (auto& c : citizens_)
        {
            if (c.sex == CitizenSex::Male && c.health > 0)
            {
                c.name = name;
                ++version_;
                return true;
            }
        }
        return false;
    }

    void SettlementCitizenState::rememberAncestry(const SettlementCitizen& c)
    {
        if (std::none_of(
                ancestors_.begin(),
                ancestors_.end(),
                [&](const auto& a) { return a.id == c.id; }
            ))
        {
            ancestors_.push_back(
                {c.id,
                 c.birthFatherId ? c.birthFatherId : c.fatherId,
                 c.birthMotherId ? c.birthMotherId : c.motherId}
            );
        }
    }

    bool SettlementCitizenState::spawn(std::uint64_t citizenCount)
    {
        if (!appendCitizens(citizenCount, false))
        {
            return false;
        }
        matchSingles();
        return true;
    }

    bool SettlementCitizenState::appendCitizens(
        std::uint64_t citizenCount,
        bool child
    )
    {
        if (!citizenCount || citizenCount > 100000 ||
            citizens_.size() > citizens_.max_size() - citizenCount)
        {
            return false;
        }
        const auto nameSeed = behaviorSeed_;
        const auto first = citizens_.size();
        const auto required = first + static_cast<std::size_t>(citizenCount);
        if (required > citizens_.capacity())
        {
            const auto growth = std::min(
                citizens_.max_size() - citizens_.capacity(),
                citizens_.capacity() / 2
            );
            citizens_.reserve(
                std::max(required, citizens_.capacity() + growth)
            );
        }

        const auto savedIds = citizenIds_;
        try
        {
            for (std::uint64_t index = first; index < first + citizenCount;
                 ++index)
            {
                const auto id = citizenIds_.generate();
                if (id.value() >= (std::uint64_t(1) << 63))
                {
                    throw std::overflow_error(
                        "Citizen entity ID namespace exhausted"
                    );
                }
                const auto sequence = id.value() - 1;
                const CitizenSex sex =
                    (GenerationNoise::mix(nameSeed ^ (sequence * 104729ULL)) &
                     1U) == 0
                        ? CitizenSex::Male
                        : CitizenSex::Female;
                const auto& names =
                    sex == CitizenSex::Male ? maleNames : femaleNames;
                const std::size_t poolIndex = static_cast<std::size_t>(
                    (nameSeed + sequence * 17U) % names.size()
                );

                citizens_.emplace_back();
                auto& citizen = citizens_.back();
                citizen.id = id;
                citizen.name = names[poolIndex];
                citizen.sex = sex;
                citizen.primaryCultureId=dominantCulture_;
                citizen.birthSettlementId=community_;
                if (naturalization_ && !child) citizen.citizenshipRealmId=communityRealm_;
                citizen.child = child;
                citizen.ageYears =
                    child
                        ? 0
                        : std::uint16_t(
                              25 + GenerationNoise::mix(
                                       nameSeed ^ citizen.id.value() ^ 0xA6EULL
                                   ) % 21
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
        ++familyVersion_;
        return true;
    }

    void SettlementCitizenState::placeUnpositionedCitizens(
        const SettlementMap& settlementMap
    )
    {
        const SettlementGrid& grid = settlementMap.grid();
        if (std::all_of(
                citizens_.begin(),
                citizens_.end(),
                [&](const auto& c)
                { return grid.isValidPosition(c.tilePosition); }
            ))
        {
            return;
        }
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

        const auto enqueue =
            [&grid, &visited, &searchFrontier](SettlementTilePosition position)
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

        for (std::int32_t y = cityKeep->footprint.topLeft.y;
             y < cityKeep->footprint.topLeft.y + cityKeep->footprint.height;
             ++y)
        {
            for (std::int32_t x = cityKeep->footprint.topLeft.x;
                 x < cityKeep->footprint.topLeft.x + cityKeep->footprint.width;
                 ++x)
            {
                enqueue({x, y});
            }
        }

        constexpr std::array<SettlementTilePosition, 4> neighborOffsets{
            {{0, -1}, {-1, 0}, {1, 0}, {0, 1}}
        };

        while (!searchFrontier.empty() && spawnTiles.size() < citizens_.size())
        {
            const SettlementTilePosition position = searchFrontier.front();
            searchFrontier.pop_front();

            if (!cityKeep->footprint.contains(position) &&
                isWalkableCitizenTile(settlementMap, position))
            {
                spawnTiles.push_back(position);
            }

            for (const SettlementTilePosition offset : neighborOffsets)
            {
                enqueue({position.x + offset.x, position.y + offset.y});
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

            citizen.tilePosition = spawnTiles[spawnIndex % spawnTiles.size()];
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
            if (citizen.child || citizen.activity != CitizenActivity::Idle)
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


    std::span<const SettlementCitizen> SettlementCitizenState::
        citizens() const noexcept
    {
        return citizens_;
    }


    const SettlementCitizen* SettlementCitizenState::citizen(
        CitizenId id
    ) const noexcept
    {
        const auto iterator = std::lower_bound(
            citizens_.begin(),
            citizens_.end(),
            id,
            [](const SettlementCitizen& citizen, CitizenId key)
            { return citizen.id < key; }
        );

        return iterator == citizens_.end() || iterator->id != id
                   ? nullptr : &*iterator;
    }

    SettlementCitizen* SettlementCitizenState::mutableCitizen(
        CitizenId id
    ) noexcept
    {
        return const_cast<SettlementCitizen*>(std::as_const(*this).citizen(id));
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
                return !citizen.militaryDeployed && SettlementTilePosition{
                           int(std::floor(citizen.visualX() + .5)),
                           int(std::floor(citizen.visualY() + .5))
                       } == position;
            }
        );

        return iterator == citizens_.rend() ? nullptr : &*iterator;
    }


    std::uint64_t SettlementCitizenState::version() const noexcept
    {
        return version_;
    }
} // namespace Paladin
