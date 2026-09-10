#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include "world/territory/TribalInfluencePolicy.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Paladin
{
    class Realm;
    class Settlement;
    class WorldGrid;

    struct TribalPowerCenterProfile
    {
        double radiusTiles = 0.0;
        double amplitude = 0.0;
    };

    // At every location the field retains the two strongest *realm* signals.
    // A realm's signal is the maximum of its individual power-center signals,
    // preventing settlement stacking from creating artificial super-strength.
    struct TribalInfluenceSample
    {
        RealmId primaryRealm;
        RealmId secondaryRealm;
        float primaryInfluence = 0.0F;
        float secondaryInfluence = 0.0F;

        [[nodiscard]]
        float influenceFor(RealmId realmId) const noexcept
        {
            if (primaryRealm == realmId)
            {
                return primaryInfluence;
            }
            if (secondaryRealm == realmId)
            {
                return secondaryInfluence;
            }
            return 0.0F;
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return !primaryRealm.isValid();
        }
    };

    [[nodiscard]]
    TribalPowerCenterProfile tribalPowerCenterProfile(
        std::uint64_t population,
        bool capital,
        const TribalInfluencePolicy& policy
    ) noexcept;

    // Compact C2 fade kernel. q is effective distance / reach:
    // K(q) = 1 - 10q^3 + 15q^4 - 6q^5 for 0 <= q < 1, else 0.
    // Both the value and slope reach zero smoothly at the frontier.
    [[nodiscard]]
    double tribalInfluenceKernel(double normalizedDistance) noexcept;

    [[nodiscard]]
    double tribalContactPressure(
        double weakerInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept;

    [[nodiscard]]
    double tribalCompetitionExponent(
        double weakerInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept;

    // Returns the primary realm's normalized color weight when two tribal
    // fields overlap. Weak contact blends broadly; strong contact increases the
    // softmax exponent and therefore converges naturally toward a sharp line.
    [[nodiscard]]
    double tribalPrimaryBlendWeight(
        double primaryInfluence,
        double secondaryInfluence,
        const TribalInfluencePolicy& policy
    ) noexcept;

    class TribalInfluenceMap
    {
    public:
        TribalInfluenceMap(std::int32_t width, std::int32_t height);

        void synchronize(
            const WorldGrid& grid,
            std::span<const Realm> realms,
            std::span<const Settlement> settlements,
            const TribalInfluencePolicy& policy
        );

        [[nodiscard]]
        std::int32_t width() const noexcept
        {
            return width_;
        }

        [[nodiscard]]
        std::int32_t height() const noexcept
        {
            return height_;
        }

        [[nodiscard]]
        TribalInfluenceSample sampleAt(WorldTilePosition position) const noexcept;

        [[nodiscard]]
        TribalInfluenceSample sampleContinuous(double x, double y) const noexcept;

        [[nodiscard]]
        float influenceAt(
            WorldTilePosition position,
            RealmId realmId
        ) const noexcept;

        [[nodiscard]]
        bool hasInfluence() const noexcept
        {
            return influencedCellCount_ != 0;
        }

        [[nodiscard]]
        std::size_t influencedCellCount() const noexcept
        {
            return influencedCellCount_;
        }

        [[nodiscard]]
        std::uint64_t revision() const noexcept
        {
            return revision_;
        }

    private:
        [[nodiscard]]
        std::size_t indexOf(WorldTilePosition position) const noexcept;

        [[nodiscard]]
        WorldTilePosition wrapped(WorldTilePosition position) const noexcept;

        void consider(
            std::size_t cellIndex,
            RealmId realmId,
            float influence
        ) noexcept;

        std::int32_t width_ = 0;
        std::int32_t height_ = 0;
        std::vector<TribalInfluenceSample> cells_;
        std::size_t influencedCellCount_ = 0;
        std::uint64_t sourceSignature_ = 0;
        std::uint64_t revision_ = 0;
    };
} // namespace Paladin
