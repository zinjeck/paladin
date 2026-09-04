#include "world/territory/TerritoryFoundationSystem.h"

#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/WorldTilePosition.h"
#include "world/territory/TerritoryFoundationPolicy.h"
#include "world/territory/TerritoryMap.h"

#include <array>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace Paladin
{
    namespace
    {
        struct ExpansionCandidate
        {
            std::uint32_t cost = 0;
            WorldTilePosition position;
        };

        struct LowestCostFirst
        {
            bool operator()(
                const ExpansionCandidate& left,
                const ExpansionCandidate& right
            ) const noexcept
            {
                return left.cost > right.cost;
            }
        };

        constexpr std::array<WorldTilePosition, 4> neighborOffsets{
            WorldTilePosition{-1, 0},
            WorldTilePosition{1, 0},
            WorldTilePosition{0, -1},
            WorldTilePosition{0, 1}
        };

        std::size_t positionIndex(
            WorldTilePosition position,
            std::int32_t width
        ) noexcept
        {
            return
                static_cast<std::size_t>(position.y)
                    * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(position.x);
        }

        std::uint32_t borderlandIrregularityCost(
            WorldTilePosition position,
            WorldTilePosition settlementPosition,
            PolityId polityId,
            const TerritoryFoundationPolicy& policy
        ) noexcept
        {
            if (policy.borderlandIrregularityMaximumCost == 0)
            {
                return 0;
            }

            std::uint64_t value = policy.borderlandShapeSalt;
            value ^= static_cast<std::uint32_t>(position.x);
            value *= 0x9E3779B185EBCA87ULL;
            value ^= static_cast<std::uint32_t>(position.y);
            value *= 0xC2B2AE3D27D4EB4FULL;
            value ^= static_cast<std::uint32_t>(
                settlementPosition.x
            );
            value *= 0x165667B19E3779F9ULL;
            value ^= static_cast<std::uint32_t>(
                settlementPosition.y
            );
            value ^= polityId.value();

            value ^= value >> 30U;
            value *= 0xBF58476D1CE4E5B9ULL;
            value ^= value >> 27U;
            value *= 0x94D049BB133111EBULL;
            value ^= value >> 31U;

            const std::uint64_t resultRange =
                static_cast<std::uint64_t>(
                    policy.borderlandIrregularityMaximumCost
                ) + 1ULL;

            return static_cast<std::uint32_t>(
                value % resultRange
            );
        }
    }


    std::size_t
    TerritoryFoundationSystem::establishSettlementTerritory(
        const WorldGrid& grid,
        TerritoryMap& territory,
        WorldTilePosition settlementPosition,
        PolityId polityId,
        const TerritoryFoundationPolicy& policy,
        std::uint32_t borderlandTraversalBudget
    ) const
    {
        if (
            !polityId.isValid() ||
            policy.settlementRegionWidth <= 0 ||
            policy.settlementRegionHeight <= 0 ||
            grid.width() != territory.width() ||
            grid.height() != territory.height()
        )
        {
            return 0;
        }

        const std::size_t previousControlledCount =
            territory.controlledTileCount(polityId);

        const std::int32_t minimumX =
            settlementPosition.x
            - policy.settlementRegionWidth / 2;

        const std::int32_t minimumY =
            settlementPosition.y
            - policy.settlementRegionHeight / 2;

        std::priority_queue<
            ExpansionCandidate,
            std::vector<ExpansionCandidate>,
            LowestCostFirst
        > frontier;

        std::vector<std::uint32_t> lowestCosts(
            grid.tileCount(),
            std::numeric_limits<std::uint32_t>::max()
        );

        for (
            std::int32_t localY = 0;
            localY < policy.settlementRegionHeight;
            ++localY
        )
        {
            for (
                std::int32_t localX = 0;
                localX < policy.settlementRegionWidth;
                ++localX
            )
            {
                const WorldTilePosition position{
                    minimumX + localX,
                    minimumY + localY
                };

                const WorldTile* tile = grid.tile(position);

                if (!tile)
                {
                    continue;
                }

                const TerritoryTerrainRule& rule =
                    policy.ruleFor(tile->terrain);

                if (!rule.controllable)
                {
                    continue;
                }

                if (!territory.claimIfUncontrolled(position, polityId))
                {
                    continue;
                }

                if (tile->terrain != TerrainType::Land)
                {
                    continue;
                }

                const std::size_t index =
                    positionIndex(position, grid.width());

                lowestCosts[index] = 0;
                frontier.push({0, position});
            }
        }

        while (!frontier.empty())
        {
            const ExpansionCandidate candidate = frontier.top();
            frontier.pop();

            const std::size_t candidateIndex =
                positionIndex(candidate.position, grid.width());

            if (candidate.cost != lowestCosts[candidateIndex])
            {
                continue;
            }

            if (candidate.cost >= borderlandTraversalBudget)
            {
                continue;
            }

            for (const WorldTilePosition offset : neighborOffsets)
            {
                const WorldTilePosition neighbor{
                    candidate.position.x + offset.x,
                    candidate.position.y + offset.y
                };

                const WorldTile* tile = grid.tile(neighbor);

                if (!tile)
                {
                    continue;
                }

                const TerritoryTerrainRule& rule =
                    policy.ruleFor(tile->terrain);

                const std::uint64_t traversalCost =
                    static_cast<std::uint64_t>(
                        rule.borderlandTraversalCost
                    )
                    + borderlandIrregularityCost(
                        neighbor,
                        settlementPosition,
                        polityId,
                        policy
                    );

                if (
                    !rule.controllable ||
                    rule.borderlandTraversalCost == 0 ||
                    traversalCost >
                        static_cast<std::uint64_t>(
                            borderlandTraversalBudget
                                - candidate.cost
                        )
                )
                {
                    continue;
                }

                const PolityId existingController =
                    territory.controllerAt(neighbor);

                if (
                    existingController.isValid() &&
                    existingController != polityId
                )
                {
                    continue;
                }

                const std::uint32_t nextCost =
                    candidate.cost
                    + static_cast<std::uint32_t>(traversalCost);

                const std::size_t neighborIndex =
                    positionIndex(neighbor, grid.width());

                if (nextCost >= lowestCosts[neighborIndex])
                {
                    continue;
                }

                lowestCosts[neighborIndex] = nextCost;
                static_cast<void>(
                    territory.claimIfUncontrolled(
                        neighbor,
                        polityId
                    )
                );
                frontier.push({nextCost, neighbor});
            }
        }

        return
            territory.controlledTileCount(polityId)
            - previousControlledCount;
    }
}
