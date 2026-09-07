#pragma once
#include <algorithm>
#include <cmath>

namespace Paladin
{
    // A continuous material field sampled at logical pixel centres. Adjacent
    // cells share the same field, so their curved edges join without seams.
    template<class Occupied>
    double surfaceField(double x, double y, const Occupied& occupied)
    {
        const int ix = int(std::floor(x - .5)), iy = int(std::floor(y - .5));
        double u = x - .5 - ix, v = y - .5 - iy;
        u = u * u * (3 - 2 * u);
        v = v * v * (3 - 2 * v);
        return std::lerp(
            std::lerp(
                double(occupied(ix, iy)),
                double(occupied(ix + 1, iy)),
                u
            ),
            std::lerp(
                double(occupied(ix, iy + 1)),
                double(occupied(ix + 1, iy + 1)),
                u
            ),
            v
        );
    }
} // namespace Paladin
