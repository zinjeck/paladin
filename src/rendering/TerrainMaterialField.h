#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include <cmath>

namespace Paladin
{
    inline std::uint32_t landscapeHash(int x, int y, unsigned salt = 0)
    {
        auto h = std::uint32_t(x) * 0x9e3779b9u ^
                 std::uint32_t(y) * 0x85ebca6bu ^ salt;
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        return h ^ (h >> 16);
    }
    inline double landscapeField(double x, double y, unsigned salt = 0)
    {
        const int ix = int(std::floor(x)), iy = int(std::floor(y));
        double u = x - ix, v = y - iy;
        u = u * u * (3 - 2 * u);
        v = v * v * (3 - 2 * v);
        const auto sample = [&](int xx, int yy)
        { return (landscapeHash(xx, yy, salt) & 65535) / 65535.; };
        return std::lerp(
            std::lerp(sample(ix, iy), sample(ix + 1, iy), u),
            std::lerp(sample(ix, iy + 1), sample(ix + 1, iy + 1), u),
            v
        );
    }
    inline bool samePaint(RenderColor a, RenderColor b)
    {
        return a.red == b.red && a.green == b.green && a.blue == b.blue &&
               a.alpha == b.alpha;
    }
    // Continuous map-space texture coordinates and coverage: no per-tile
    // reseeding, zoom reseeding or repeated rectangles of identical density.
    inline RenderColor landscapePaint(
        const SceneSprite& art,
        double x,
        double y,
        bool world
    )
    {
        if (!art.materialPixels || art.materialPixels->empty())
        {
            return art.overviewColor;
        }
        const double broad = landscapeField(x * .055, y * .055, 71);
        const double cover = landscapeField(x * .19, y * .19, 331);
        const double detail = landscapeField(x * 2.7, y * 2.7, 991);
        // Coverage varies gently but never removes all grass texture from a
        // region. Quiet ground still has small clusters and colored shadows.
        const double density = world ? .58 + broad * .30 : .72 + broad * .28;
        if (detail > density)
        {
            return art.materialBase;
        }
        // Gentle shifts preserve the artist's clusters. Large derivatives
        // fold source coordinates into stretched, repeated streaks.
        const double warpX =
            landscapeField(x * .22, y * .22, 121) * 10 + cover * 5;
        const double warpY =
            landscapeField(x * .22, y * .22, 717) * 10 + broad * 5;
        const int px = int(std::floor(x * 16 + warpX)),
                  py = int(std::floor(y * 16 + warpY));
        const int xx =
            (px % art.materialWidth + art.materialWidth) % art.materialWidth;
        const int yy =
            (py % art.materialHeight + art.materialHeight) % art.materialHeight;
        return (*art.materialPixels)[std::size_t(yy) * art.materialWidth + xx];
    }
} // namespace Paladin
