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
        // Inputs are finite and interpolation weights are in [0,1]. Direct
        // bilinear arithmetic avoids repeated generic lerp dispatch in Debug.
        const double a = (landscapeHash(ix, iy, salt) & 65535) / 65535.,
                     b = (landscapeHash(ix + 1, iy, salt) & 65535) / 65535.,
                     c = (landscapeHash(ix, iy + 1, salt) & 65535) / 65535.,
                     d = (landscapeHash(ix + 1, iy + 1, salt) & 65535) / 65535.;
        const double upper = a + (b - a) * u, lower = c + (d - c) * u;
        return upper + (lower - upper) * v;
    }
    inline bool samePaint(RenderColor a, RenderColor b)
    {
        return a.red == b.red && a.green == b.green && a.blue == b.blue &&
               a.alpha == b.alpha;
    }
    // Continuous map-space texture coordinates and coverage: no per-tile
    // reseeding, zoom reseeding or repeated rectangles of identical density.
    RenderColor landscapePaint(
        const SceneSprite& art,
        double x,
        double y,
        bool world
    );
} // namespace Paladin
