#include "world/territory/TribalInfluenceMap.h"

#include "world/Realm.h"
#include "world/Settlement.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace Paladin
{
    namespace
    {
        struct PropagationCandidate
        {
            float cost = 0.0F;
            WorldTilePosition position;
        };

        struct LowestCostFirst
        {
            bool operator()(
                const PropagationCandidate& left,
                const PropagationCandidate& right
            ) const noexcept
            {
                return left.cost > right.cost;
            }
        };

        struct PropagationStep
        {
            int dx = 0;
            int dy = 0;
            float length = 1.0F;
        };

        constexpr float RootTwo = 1.4142135623730950488F;
        constexpr std::array<PropagationStep, 8> PropagationSteps{
            PropagationStep{-1, 0, 1.0F},
            PropagationStep{1, 0, 1.0F},
            PropagationStep{0, -1, 1.0F},
            PropagationStep{0, 1, 1.0F},
            PropagationStep{-1, -1, RootTwo},
            PropagationStep{1, -1, RootTwo},
            PropagationStep{-1, 1, RootTwo},
            PropagationStep{1, 1, RootTwo}
        };

        std::uint64_t mixSignature(
            std::uint64_t signature,
            std::uint64_t value
        ) noexcept
        {
            signature ^= value;
            signature *= 1099511628211ULL;
            return signature;
        }

        std::uint64_t doubleBits(double value) noexcept
        {
            return std::bit_cast<std::uint64_t>(value);
        }

        std::uint64_t policySignature(const TribalInfluencePolicy& policy) noexcept
        {
            std::uint64_t signature = 1469598103934665603ULL;
            signature = mixSignature(
                signature,
                doubleBits(policy.referencePopulation)
            );
            signature = mixSignature(signature, doubleBits(policy.baseRadiusTiles));
            signature = mixSignature(
                signature,
                doubleBits(policy.populationRadiusTiles)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.capitalRadiusMultiplier)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.capitalAmplitudeBonus)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.hillsResistance)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.mountainResistance)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.visibleInfluenceThreshold)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.firmContactBegin)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.firmContactFull)
            );
            signature = mixSignature(
                signature,
                doubleBits(policy.maximumCompetitionExponent)
            );
            return signature;
        }

        float terrainResistance(
            const WorldTile& tile,
            const TribalInfluencePolicy& policy
        ) noexcept
        {
            if (tile.terrain == TerrainType::Mountain ||
                tile.relief == ReliefType::Mountain)
            {
                return static_cast<float>(
                    std::max(1.0, policy.mountainResistance)
                );
            }
            if (tile.relief == ReliefType::Hills)
            {
                return static_cast<float>(
                    std::max(1.0, policy.hillsResistance)
                );
            }
            return 1.0F;
        }

        double smoothStep(double edge0, double edge1, double value) noexcept
        {
            if (!(edge1 > edge0))
            {
                return value >= edge1 ? 1.0 : 0.0;
            }
            const double t = std::clamp(
                (value - edge0) / (edge1 - edge0),
                0.0,
                1.0
            );
            return t * t * (3.0 - 2.0 * t);
        }

        const Realm* findRealm(
            std::span<const Realm> realms,
            RealmId realmId
        ) noexcept
        {
            for (const Realm& realm : realms)
            {
                if (realm.id() == realmId)
                {
                    return &realm;
                }
            }
            return nullptr;
        }
    } // namespace


    TribalPowerCenterProfile tribalPowerCenterProfile(
        std::uint64_t population,
        bool capital,
        const TribalInfluencePolicy& policy
    ) noexcept
    {
        if (population == 0)
        {
            return {};
        }

        const double referencePopulation =
            std::max(1.0, policy.referencePopulation);
        const double populationRatio =
            static_cast<double>(population) / referencePopulation;

        double radius =
            std::max(0.0, policy.baseRadiusTiles) +
            std::max(0.0, policy.populationRadiusTiles) *
                std::log2(1.0 + populationRatio);

        if (capital)
        {
            radius *= std::max(1.0, policy.capitalRadiusMultiplier);
        }

        double amplitude = 1.0 - std::exp(-std::sqrt(populationRatio));
        if (capital)
        {
            amplitude += std::max(0.0, policy.capitalAmplitudeBonus);
        }

        return {
            radius,
            std::clamp(amplitude, 0.0, 1.0)
        };
    }


    double tribalInfluenceKernel(double normalizedDistance) noexcept
    {
        if (!std::isfinite(normalizedDistance) || normalizedDistance >= 1.0)
        {
            return 0.0;
        }
        if (normalizedDistance <= 0.0)
        {
            return 1.0;
        }

        const double q = normalizedDistance;
        const double q2 = q * q;
        const double q3 = q2 * q;
        const double q4 = q3 * q;
        const double q5 = q4 * q;
        return std::clamp(1.0 - 10.0 * q3 + 15.0 * q4 - 6.0 * q5, 0.0, 1.0);
    }


    double tribalContactPressure(
        double weakerInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept
    {
        return smoothStep(
            std::max(0.0, policy.firmContactBegin),
            std::max(
                std::max(0.0, policy.firmContactBegin) + 1e-6,
                policy.firmContactFull
            ),
            std::max(0.0, weakerInfluence)
        );
    }


    double tribalCompetitionExponent(
        double weakerInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept
    {
        const double pressure = tribalContactPressure(weakerInfluence, policy);
        const double maximumExponent =
            std::max(1.0, policy.maximumCompetitionExponent);
        return 1.0 + (maximumExponent - 1.0) * pressure * pressure;
    }


    double tribalPrimaryBlendWeight(
        double primaryInfluence,
        double secondaryInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept
    {
        const double primary = std::max(0.0, primaryInfluence);
        const double secondary = std::max(0.0, secondaryInfluence);
        if (primary <= 0.0)
        {
            return 0.0;
        }
        if (secondary <= 0.0)
        {
            return 1.0;
        }

        const double exponent = tribalCompetitionExponent(
            std::min(primary, secondary),
            policy
        );
        const double primaryWeight = std::pow(primary, exponent);
        const double secondaryWeight = std::pow(secondary, exponent);
        const double total = primaryWeight + secondaryWeight;
        return total > 0.0 ? primaryWeight / total : 0.5;
    }


    TribalInfluenceMap::TribalInfluenceMap(
        std::int32_t width,
        std::int32_t height
    )
        : width_(width), height_(height)
    {
        if (width_ <= 0 || height_ <= 0)
        {
            throw std::invalid_argument(
                "TribalInfluenceMap dimensions must be positive."
            );
        }
        cells_.resize(
            static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_)
        );
    }


    void TribalInfluenceMap::synchronize(
        const WorldGrid& grid,
        std::span<const Realm> realms,
        std::span<const Settlement> settlements,
        const TribalInfluencePolicy& policy
    )
    {
        if (grid.width() != width_ || grid.height() != height_)
        {
            return;
        }

        std::uint64_t signature = policySignature(policy);
        signature = mixSignature(signature, grid.revision());
        signature = mixSignature(signature, static_cast<std::uint64_t>(width_));
        signature = mixSignature(signature, static_cast<std::uint64_t>(height_));

        for (const Realm& realm : realms)
        {
            signature = mixSignature(signature, realm.id().value());
            signature = mixSignature(
                signature,
                realm.startingOriginId() == "tribal" ? 1ULL : 0ULL
            );
            signature = mixSignature(
                signature,
                realm.capitalSettlementId().value()
            );
        }

        for (const Settlement& settlement : settlements)
        {
            signature = mixSignature(signature, settlement.id().value());
            signature = mixSignature(signature, settlement.ownerRealmId().value());
            signature = mixSignature(
                signature,
                static_cast<std::uint32_t>(settlement.position().x)
            );
            signature = mixSignature(
                signature,
                static_cast<std::uint32_t>(settlement.position().y)
            );
            signature = mixSignature(signature, settlement.population());
            signature = mixSignature(
                signature,
                settlement.simulationState().population().version()
            );
        }

        if (sourceSignature_ == signature)
        {
            return;
        }

        sourceSignature_ = signature;
        std::fill(cells_.begin(), cells_.end(), TribalInfluenceSample{});
        influencedCellCount_ = 0;

        const std::size_t tileCount = cells_.size();
        std::vector<float> distances(tileCount, 0.0F);
        std::vector<std::uint32_t> distanceGeneration(tileCount, 0U);
        std::uint32_t generation = 0;

        for (const Settlement& settlement : settlements)
        {
            const Realm* realm = findRealm(realms, settlement.ownerRealmId());
            if (!realm || realm->startingOriginId() != "tribal")
            {
                continue;
            }

            const WorldTilePosition center = wrapped(settlement.position());
            if (center.y < 0 || center.y >= height_)
            {
                continue;
            }
            const WorldTile* centerTile = grid.tile(center);
            if (!centerTile || centerTile->terrain == TerrainType::Water)
            {
                continue;
            }

            const TribalPowerCenterProfile profile = tribalPowerCenterProfile(
                settlement.population(),
                realm->capitalSettlementId() == settlement.id(),
                policy
            );
            if (profile.radiusTiles <= 0.0 || profile.amplitude <= 0.0)
            {
                continue;
            }

            ++generation;
            if (generation == 0)
            {
                std::fill(
                    distanceGeneration.begin(),
                    distanceGeneration.end(),
                    0U
                );
                generation = 1;
            }

            std::priority_queue<
                PropagationCandidate,
                std::vector<PropagationCandidate>,
                LowestCostFirst>
                frontier;

            const std::size_t centerIndex = indexOf(center);
            distanceGeneration[centerIndex] = generation;
            distances[centerIndex] = 0.0F;
            frontier.push({0.0F, center});

            while (!frontier.empty())
            {
                const PropagationCandidate candidate = frontier.top();
                frontier.pop();

                const WorldTilePosition position = wrapped(candidate.position);
                const std::size_t index = indexOf(position);
                if (distanceGeneration[index] != generation ||
                    candidate.cost != distances[index])
                {
                    continue;
                }

                if (candidate.cost > profile.radiusTiles)
                {
                    continue;
                }

                const double normalizedDistance =
                    static_cast<double>(candidate.cost) / profile.radiusTiles;
                const float influence = static_cast<float>(
                    profile.amplitude *
                    tribalInfluenceKernel(normalizedDistance)
                );
                if (influence > 0.0F)
                {
                    consider(index, realm->id(), influence);
                }

                if (candidate.cost >= profile.radiusTiles)
                {
                    continue;
                }

                const WorldTile* sourceTile = grid.tile(position);
                if (!sourceTile)
                {
                    continue;
                }
                const float sourceResistance = terrainResistance(
                    *sourceTile,
                    policy
                );

                for (const PropagationStep& step : PropagationSteps)
                {
                    WorldTilePosition neighbor{
                        position.x + step.dx,
                        position.y + step.dy
                    };
                    if (neighbor.y < 0 || neighbor.y >= height_)
                    {
                        continue;
                    }
                    neighbor = wrapped(neighbor);

                    const WorldTile* targetTile = grid.tile(neighbor);
                    if (!targetTile || targetTile->terrain == TerrainType::Water)
                    {
                        continue;
                    }

                    const float targetResistance = terrainResistance(
                        *targetTile,
                        policy
                    );
                    const float traversalCost =
                        step.length * (sourceResistance + targetResistance) *
                        0.5F;
                    const float nextCost = candidate.cost + traversalCost;
                    if (nextCost > profile.radiusTiles)
                    {
                        continue;
                    }

                    const std::size_t neighborIndex = indexOf(neighbor);
                    const bool unseen =
                        distanceGeneration[neighborIndex] != generation;
                    if (!unseen && nextCost >= distances[neighborIndex])
                    {
                        continue;
                    }

                    distanceGeneration[neighborIndex] = generation;
                    distances[neighborIndex] = nextCost;
                    frontier.push({nextCost, neighbor});
                }
            }
        }

        influencedCellCount_ = static_cast<std::size_t>(std::count_if(
            cells_.begin(),
            cells_.end(),
            [](const TribalInfluenceSample& sample)
            { return sample.primaryRealm.isValid() && sample.primaryInfluence > 0.0F; }
        ));
        ++revision_;
    }


    TribalInfluenceSample TribalInfluenceMap::sampleAt(
        WorldTilePosition position
    ) const noexcept
    {
        if (position.y < 0 || position.y >= height_ || width_ <= 0)
        {
            return {};
        }
        position = wrapped(position);
        return cells_[indexOf(position)];
    }


    TribalInfluenceSample TribalInfluenceMap::sampleContinuous(
        double x,
        double y
    ) const noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(y) || width_ <= 0 ||
            height_ <= 0 || y < 0.0 || y >= static_cast<double>(height_))
        {
            return {};
        }

        double wrappedX = std::fmod(x, static_cast<double>(width_));
        if (wrappedX < 0.0)
        {
            wrappedX += static_cast<double>(width_);
        }

        const double sampleX = wrappedX - 0.5;
        const double sampleY = y - 0.5;
        const int x0 = static_cast<int>(std::floor(sampleX));
        const int y0 = static_cast<int>(std::floor(sampleY));
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;
        const double tx = sampleX - std::floor(sampleX);
        const double ty = sampleY - std::floor(sampleY);

        const auto at = [this](int px, int py)
        {
            py = std::clamp(py, 0, height_ - 1);
            return sampleAt({px, py});
        };

        const TribalInfluenceSample samples[4]{
            at(x0, y0),
            at(x1, y0),
            at(x0, y1),
            at(x1, y1)
        };
        const double weights[4]{
            (1.0 - tx) * (1.0 - ty),
            tx * (1.0 - ty),
            (1.0 - tx) * ty,
            tx * ty
        };

        std::array<RealmId, 8> candidates{};
        std::size_t candidateCount = 0;
        const auto addCandidate = [&](RealmId realmId)
        {
            if (!realmId.isValid())
            {
                return;
            }
            for (std::size_t i = 0; i < candidateCount; ++i)
            {
                if (candidates[i] == realmId)
                {
                    return;
                }
            }
            candidates[candidateCount++] = realmId;
        };

        for (const TribalInfluenceSample& sample : samples)
        {
            addCandidate(sample.primaryRealm);
            addCandidate(sample.secondaryRealm);
        }

        TribalInfluenceSample result;
        for (std::size_t candidateIndex = 0;
             candidateIndex < candidateCount;
             ++candidateIndex)
        {
            const RealmId realmId = candidates[candidateIndex];
            double value = 0.0;
            for (std::size_t sampleIndex = 0; sampleIndex < 4; ++sampleIndex)
            {
                value += weights[sampleIndex] *
                         samples[sampleIndex].influenceFor(realmId);
            }
            const float influence = static_cast<float>(value);
            if (influence <= 0.0F)
            {
                continue;
            }

            const bool precedesPrimary =
                influence > result.primaryInfluence ||
                (influence == result.primaryInfluence &&
                 (!result.primaryRealm.isValid() ||
                  realmId.value() < result.primaryRealm.value()));
            if (precedesPrimary)
            {
                if (result.primaryRealm != realmId)
                {
                    result.secondaryRealm = result.primaryRealm;
                    result.secondaryInfluence = result.primaryInfluence;
                }
                result.primaryRealm = realmId;
                result.primaryInfluence = influence;
            }
            else if (realmId != result.primaryRealm &&
                     (influence > result.secondaryInfluence ||
                      (influence == result.secondaryInfluence &&
                       (!result.secondaryRealm.isValid() ||
                        realmId.value() < result.secondaryRealm.value()))))
            {
                result.secondaryRealm = realmId;
                result.secondaryInfluence = influence;
            }
        }

        return result;
    }


    float TribalInfluenceMap::influenceAt(
        WorldTilePosition position,
        RealmId realmId
    ) const noexcept
    {
        return sampleAt(position).influenceFor(realmId);
    }


    std::size_t TribalInfluenceMap::indexOf(
        WorldTilePosition position
    ) const noexcept
    {
        return static_cast<std::size_t>(position.y) *
                   static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(position.x);
    }


    WorldTilePosition TribalInfluenceMap::wrapped(
        WorldTilePosition position
    ) const noexcept
    {
        if (width_ > 0)
        {
            position.x %= width_;
            if (position.x < 0)
            {
                position.x += width_;
            }
        }
        return position;
    }


    void TribalInfluenceMap::consider(
        std::size_t cellIndex,
        RealmId realmId,
        float influence
    ) noexcept
    {
        if (!realmId.isValid() || influence <= 0.0F ||
            cellIndex >= cells_.size())
        {
            return;
        }

        TribalInfluenceSample& sample = cells_[cellIndex];
        if (sample.primaryRealm == realmId)
        {
            sample.primaryInfluence = std::max(
                sample.primaryInfluence,
                influence
            );
            return;
        }
        if (sample.secondaryRealm == realmId)
        {
            sample.secondaryInfluence = std::max(
                sample.secondaryInfluence,
                influence
            );
            if (sample.secondaryInfluence > sample.primaryInfluence)
            {
                std::swap(sample.primaryRealm, sample.secondaryRealm);
                std::swap(sample.primaryInfluence, sample.secondaryInfluence);
            }
            return;
        }

        const bool becomesPrimary =
            influence > sample.primaryInfluence ||
            (influence == sample.primaryInfluence &&
             (!sample.primaryRealm.isValid() ||
              realmId.value() < sample.primaryRealm.value()));
        if (becomesPrimary)
        {
            sample.secondaryRealm = sample.primaryRealm;
            sample.secondaryInfluence = sample.primaryInfluence;
            sample.primaryRealm = realmId;
            sample.primaryInfluence = influence;
            return;
        }

        if (influence > sample.secondaryInfluence ||
            (influence == sample.secondaryInfluence &&
             (!sample.secondaryRealm.isValid() ||
              realmId.value() < sample.secondaryRealm.value())))
        {
            sample.secondaryRealm = realmId;
            sample.secondaryInfluence = influence;
        }
    }
} // namespace Paladin
