#pragma once
#include <algorithm>
#include <cmath>

namespace Paladin
{
    struct CoastSample
    {
        double x, y;
    };

    // Continuous, bounded displacement in map space. It is independent of the
    // camera, cache chunks and time: waves move, the land itself does not.
    inline CoastSample coastSample(double x, double y, bool worldScale)
    {
        const double scale = worldScale ? .55 : 1.7;
        const double amplitude = worldScale ? .12 : .19;
        return {
            x + amplitude * std::sin(y * scale + .6 * std::sin(x * .71)) +
                amplitude * .28 * std::sin(y * scale * 3.1 + x * 1.3),
            y +
                amplitude *
                    std::sin(x * scale * 1.13 + .7 * std::sin(y * .63)) +
                amplitude * .28 * std::sin(x * scale * 2.7 - y * 1.1)
        };
    }

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

    // Stable settlement-space variation for road shoulders. Multiple spatial
    // frequencies keep long runs and intersections from reading as ruler-cut
    // geometry while remaining continuous across object/cache boundaries.
    inline double roadSurfaceNoise(double x, double y)
    {
        return .052 *
                   std::sin(x * 2.17 + y * .83 + .35 * std::sin(y * .61)) +
               .026 * std::sin(x * 6.7 - y * 5.1) +
               .012 * std::sin(x * 13.7 + y * 11.3);
    }

    inline int roadSurfaceOpacity(double field, double x, double y)
    {
        // Never let edge variation punch holes through the readable road core.
        if (field >= .74)
        {
            return 255;
        }
        const double coverage = std::clamp(
            (field - .44 + roadSurfaceNoise(x, y)) / .15,
            0.,
            1.
        );
        return int(coverage * 4 + .5) * 255 / 4;
    }
} // namespace Paladin
