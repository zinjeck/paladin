#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/SettlementMap.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>
namespace Paladin
{
    double SettlementCitizen::visualX() const noexcept
    {
        return tilePosition.x + (pathIndex < path.size()
            ? (path[pathIndex].x - tilePosition.x) * std::clamp(stepProgress / stepDuration, 0.0, 1.0) : 0);
    }
    double SettlementCitizen::visualY() const noexcept
    {
        return tilePosition.y + (pathIndex < path.size()
            ? (path[pathIndex].y - tilePosition.y) * std::clamp(stepProgress / stepDuration, 0.0, 1.0) : 0);
    }
    void SettlementCitizenState::resetLocalPlacement() noexcept
    {
        navigation_ = {};
        decisionCursor_ = 0;
        for (auto& citizen : citizens_)
        {
            citizen.tilePosition = {-1, -1};
            citizen.idleAnchor = {-1, -1};
            citizen.destination = {-1, -1};
            citizen.path.clear();
            citizen.pathIndex = 0;
            citizen.stepProgress = 0;
            citizen.idleWait = -1;
            citizen.explicitMovement = false;
            citizen.assignedCommandId = {};
        citizen.workplaceId = {};
        citizen.nextWorkCheckMinutes = 0;
            citizen.activity = CitizenActivity::Idle;
        }
        ++version_;
    }
    bool SettlementCitizenState::moveTo(CitizenId id, const SettlementMap& map,
        SettlementTilePosition destination)
    {
        navigation_.synchronize(map);
        for (auto& citizen : citizens_)
        {
            if (citizen.id != id) continue;
            auto path = navigation_.findPath(map, citizen.tilePosition, destination, movementPolicy);
            if (path.empty()) return citizen.tilePosition == destination;
            citizen.path = std::move(path);
            citizen.pathIndex = 0;
            citizen.stepProgress = 0;
            citizen.stepDuration = navigation_.stepCost(map, citizen.tilePosition,
                citizen.path.front(), movementPolicy);
            citizen.destination = destination;
            citizen.explicitMovement = true;
            ++version_;
            return true;
        }
        return false;
    }
    void SettlementCitizenState::tickMovement(const SettlementMap& map, double minutes)
    {
        if (!std::isfinite(minutes) || minutes <= 0 || citizens_.empty()
            || !std::isfinite(movementPolicy.tilesPerGameMinute)
            || movementPolicy.tilesPerGameMinute <= 0
            || !std::isfinite(movementPolicy.roadSpeedMultiplier)
            || movementPolicy.roadSpeedMultiplier <= 0
            || !std::isfinite(movementPolicy.diagonalCost)
            || movementPolicy.diagonalCost < 1 || movementPolicy.diagonalCost > 2) return;
        navigation_.synchronize(map);
        std::size_t requests = 0;
        for (auto& citizen : citizens_)
        {
            if (!map.grid().isValidPosition(citizen.tilePosition)) continue;
            if (citizen.idleWait >= 0) citizen.idleWait = std::max(0.0, citizen.idleWait - minutes);
            double travel = minutes * movementPolicy.tilesPerGameMinute;
            while (citizen.pathIndex < citizen.path.size())
            {
                const auto next = citizen.path[citizen.pathIndex];
                if (!navigation_.canStep(map, citizen.tilePosition, next))
                {
                    citizen.stepProgress = 0;
                    if (requests >= movementPolicy.pathRequestsPerTick) break;
                    ++requests;
                    citizen.path = navigation_.findPath(map, citizen.tilePosition,
                        citizen.destination, movementPolicy);
                    citizen.pathIndex = 0;
                    if (citizen.path.empty())
                    {
                        citizen.explicitMovement = false;
                        citizen.idleWait = -1;
                    }
                    break;
                }
                citizen.stepDuration = navigation_.stepCost(map, citizen.tilePosition, next, movementPolicy);
                const double used = std::min(travel, citizen.stepDuration - citizen.stepProgress);
                citizen.stepProgress += used;
                travel -= used;
                if (citizen.stepProgress + 1e-10 < citizen.stepDuration) break;
                citizen.tilePosition = next;
                citizen.stepProgress = 0;
                ++citizen.pathIndex;
                if (travel <= 0) break;
            }
            if (!citizen.path.empty() && citizen.pathIndex == citizen.path.size())
            {
                citizen.path.clear();
                citizen.pathIndex = 0;
                citizen.explicitMovement = false;
                citizen.idleWait = -1;
            }
        }
        const auto count = std::min(idlePolicy.decisionsPerTick, citizens_.size());
        for (std::size_t scan = 0; scan < count; ++scan)
        {
            auto& citizen = citizens_[decisionCursor_++ % citizens_.size()];
            if (!map.grid().isValidPosition(citizen.tilePosition)
                || citizen.activity != CitizenActivity::Idle || !citizen.path.empty()) continue;
            const auto random = [&](std::uint64_t salt)
            {
                return (GenerationNoise::mix(behaviorSeed_ ^ (citizen.id.value() * 104729)
                    ^ (citizen.choiceSequence * 130363) ^ salt) >> 11) * 0x1.0p-53;
            };
            const auto schedule = [&]()
            {
                const double minimum = std::max(.01, idlePolicy.minimumWaitMinutes);
                citizen.idleWait = std::lerp(minimum,
                    std::max(minimum, idlePolicy.maximumWaitMinutes), random(17));
            };
            if (citizen.idleWait == -1)
            {
                schedule();
                continue;
            }
            if (citizen.idleWait > 0) continue;
            if (requests >= movementPolicy.pathRequestsPerTick) continue;
            ++citizen.choiceSequence;
            schedule();
            if (random(31) < idlePolicy.standProbability) continue;
            if (!map.grid().isValidPosition(citizen.idleAnchor)
                || std::abs(citizen.idleAnchor.x - citizen.tilePosition.x)
                    + std::abs(citizen.idleAnchor.y - citizen.tilePosition.y)
                        > idlePolicy.anchorRadius * 2)
                citizen.idleAnchor = citizen.tilePosition;
            const int radius = std::max(0, idlePolicy.destinationRadius);
            const int diameter = radius * 2 + 1;
            const SettlementTilePosition goal{
                citizen.tilePosition.x + int(random(47) * diameter) - radius,
                citizen.tilePosition.y + int(random(53) * diameter) - radius};
            if (std::abs(goal.x - citizen.idleAnchor.x) + std::abs(goal.y - citizen.idleAnchor.y)
                > idlePolicy.anchorRadius || !navigation_.walkable(map, goal)) continue;
            CitizenMovementPolicy searchPolicy = movementPolicy;
            searchPolicy.maximumExpandedNodes = idlePolicy.maximumExpandedNodes;
            ++requests;
            auto path = navigation_.findPath(map, citizen.tilePosition, goal, searchPolicy);
            if (path.empty() || path.size() > idlePolicy.maximumPathSteps) continue;
            citizen.path = std::move(path);
            citizen.pathIndex = 0;
            citizen.stepProgress = 0;
            citizen.stepDuration = navigation_.stepCost(map, citizen.tilePosition,
                citizen.path.front(), movementPolicy);
            citizen.destination = goal;
        }
        ++version_;
    }
}
