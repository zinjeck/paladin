#pragma once

namespace Paladin
{
    // Tribal territory is a continuous influence field rather than binary
    // sovereignty. Population increases both a power center's reach and its
    // peak authority; terrain increases effective propagation distance.
    struct TribalInfluencePolicy
    {
        // R(P) = capitalMultiplier *
        //        (baseRadius + populationRadius * log2(1 + P/referencePopulation))
        double referencePopulation = 100.0;
        double baseRadiusTiles = 9.0;
        double populationRadiusTiles = 5.5;
        double capitalRadiusMultiplier = 1.20;

        // A(P) = clamp(1 - exp(-sqrt(P/referencePopulation)) + capitalBonus,
        //              0, 1)
        double capitalAmplitudeBonus = 0.08;

        // Least-cost propagation resistance. Water is intentionally impassable.
        double hillsResistance = 1.18;
        double mountainResistance = 1.65;

        // Influence below this level is visually negligible and is treated as
        // frontier haze rather than meaningful political reach.
        double visibleInfluenceThreshold = 0.025;

        // Where two tribal influence fields overlap, competition sharpens
        // continuously with the weaker field. There is no hardcoded border
        // segment state. Strong/strong contact simply produces a steep blend.
        double firmContactBegin = 0.24;
        double firmContactFull = 0.62;
        double maximumCompetitionExponent = 12.0;
    };
} // namespace Paladin
