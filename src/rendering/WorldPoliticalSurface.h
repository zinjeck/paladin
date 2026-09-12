#pragma once

#include "rendering/NaturalSurfaceShape.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    // The terrain atlas and political presentation MUST sample the same warped
    // land field. Logical controller cells remain simulation data, not polygons.
    inline double worldLandField(const WorldGrid& grid, double x, double y)
    {
        const int w = grid.width(), h = grid.height();
        if (w <= 0 || h <= 0) return 0.0;
        return surfaceField(x, y, [&](int ix, int iy) {
            return grid.tile({(ix % w + w) % w, std::clamp(iy, 0, h - 1)})
                       ->terrain != TerrainType::Water;
        });
    }

    struct WorldPoliticalSurfaceSample
    {
        bool land = false;
        RealmId civic;
        std::array<WorldTilePosition, 4> positions{};
        std::array<double, 4> dryWeights{};
        double landWeight = 0.0;
    };

    inline WorldPoliticalSurfaceSample worldPoliticalSurfaceAt(
        const World& world, double x, double y)
    {
        WorldPoliticalSurfaceSample result;
        const auto& grid = world.grid();
        const int w = grid.width(), h = grid.height();
        if (w <= 0 || h <= 0 || !std::isfinite(x) || !std::isfinite(y) ||
            y < 0.0 || y >= h) return result;
        x -= std::floor(x / w) * w;
        const auto warped = coastSample(x, y, true);
        const int ix = int(std::floor(warped.x - .5));
        const int iy = int(std::floor(warped.y - .5));
        double u = warped.x - .5 - ix, v = warped.y - .5 - iy;
        u = u * u * (3 - 2 * u);
        v = v * v * (3 - 2 * v);
        std::array<RealmId, 4> owners{};
        for (int j = 0; j != 2; ++j)
        {
            for (int i = 0; i != 2; ++i)
            {
                const int k = j * 2 + i;
                const WorldTilePosition p{
                    ((ix + i) % w + w) % w, std::clamp(iy + j, 0, h - 1)};
                result.positions[k] = p;
                if (grid.tile(p)->terrain == TerrainType::Water) continue;
                const double weight = (i ? u : 1 - u) * (j ? v : 1 - v);
                result.dryWeights[k] = weight;
                result.landWeight += weight;
                const RealmId id = world.territory().controllerAt(p);
                const auto* realm = world.realm(id);
                if (realm && !realm->usesTribalInfluence()) owners[k] = id;
            }
        }
        // Use the identical expression/order as the terrain for threshold ties.
        result.land = worldLandField(grid, warped.x, warped.y) >= .5;
        if (!result.land) return result;

        // Compare realm weights with unclaimed DRY land, not ocean. A controlled
        // island must reach its visible shoreline, including its rounded corners.
        double best = -1.0;
        for (int i = 0; i != 4; ++i)
        {
            double weight = 0;
            for (int j = 0; j != 4; ++j)
                if (owners[i] == owners[j]) weight += result.dryWeights[j];
            if (weight > best ||
                (weight == best && owners[i].value() < result.civic.value()))
            {
                best = weight;
                result.civic = owners[i];
            }
        }
        return result;
    }

    // Interpolate actual authority from the same dry surface samples. This does
    // not award tribal controller cells or change any influence equation. It
    // only avoids clipping a tribe at the OLD square coastline during display.
    inline TribalInfluenceSample worldTribalSurfaceSample(
        const TribalInfluenceMap& influence,
        const WorldPoliticalSurfaceSample& surface)
    {
        TribalInfluenceSample result{};
        if (!surface.land || surface.civic || surface.landWeight <= 0) return result;
        struct Entry { RealmId id; double value = 0; };
        std::array<Entry, 8> entries{};
        int count = 0;
        const auto add = [&](RealmId id, double value) {
            if (!id || value <= 0) return;
            for (int i = 0; i < count; ++i)
            {
                if (entries[i].id == id) { entries[i].value += value; return; }
            }
            entries[count++] = {id, value};
        };
        for (int k = 0; k != 4; ++k)
        {
            if (surface.dryWeights[k] <= 0) continue;
            const auto sample = influence.sampleAt(surface.positions[k]);
            const double weight = surface.dryWeights[k] / surface.landWeight;
            add(sample.primaryRealm, sample.primaryInfluence * weight);
            add(sample.secondaryRealm, sample.secondaryInfluence * weight);
        }
        std::sort(entries.begin(), entries.begin() + count, [](auto a, auto b) {
            return a.value != b.value ? a.value > b.value : a.id.value() < b.id.value();
        });
        if (count) { result.primaryRealm = entries[0].id; result.primaryInfluence = entries[0].value; }
        if (count > 1) { result.secondaryRealm = entries[1].id; result.secondaryInfluence = entries[1].value; }
        return result;
    }
} // namespace Paladin
