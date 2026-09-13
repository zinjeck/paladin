#pragma once

#include "core/StrongId.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Paladin
{
    // A single coordinate on each spectrum makes contradictory extremes
    // impossible.
    class RulerAxis
    {
    public:
        explicit RulerAxis(double value = .5)
            : value_(std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : .5)
        {
        }
        double first() const noexcept
        {
            return value_;
        }
        double opposite() const noexcept
        {
            return 1.0 - value_;
        }

    private:
        double value_;
    };

    struct RulerPersonality
    {
        RulerAxis militarism, isolationism, authoritarianism, elitism;
        double pacifism() const
        {
            return militarism.opposite();
        }
        double mercantilism() const
        {
            return isolationism.opposite();
        }
        double libertarianism() const
        {
            return authoritarianism.opposite();
        }
        double egalitarianism() const
        {
            return elitism.opposite();
        }
    };

    enum class RealmScale : std::uint8_t
    {
        Small,
        Medium,
        Empire
    };

    struct RulerCitizenReference
    {
        SettlementId settlement;
        CitizenId citizen;
        explicit operator bool() const noexcept
        {
            return bool(settlement) && bool(citizen);
        }
        friend bool operator==(
            const RulerCitizenReference&,
            const RulerCitizenReference&
        ) = default;
    };

    // AI courts retain only genealogy and age, never full settlement citizens.
    struct DynasticPerson
    {
        std::uint64_t id = 0, father = 0;
        std::string name;
        double age = 0, lifespan = 75, nextChildAge = 25;
        bool alive = true;
    };

    struct RealmRuler
    {
        RulerCitizenReference citizen;
        std::string name;
        RulerPersonality personality;
        double age = 0;
        std::uint64_t dynasty = 0, reign = 0;
        bool vacant = true;
        // RNG belongs to the realm; frame cadence never changes succession.
        std::uint64_t randomState = 1, nextPerson = 1, aiRuler = 0;
        double pendingMinutes = 0;
        std::vector<DynasticPerson> family;
    };
} // namespace Paladin
