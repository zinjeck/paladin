#include "rendering/TerrainMaterialField.h"
namespace Paladin
{
    RenderColor landscapePaint(
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
        const bool cityGrass = !world &&
                               art.materialBase.green > art.materialBase.red &&
                               art.materialBase.green > art.materialBase.blue;
        const double broad = landscapeField(x * .055, y * .055, 71);
        const double cover = landscapeField(x * .19, y * .19, 331);
        const double detail = landscapeField(x * 2.7, y * 2.7, 991);
        // Coverage varies gently but never removes all grass texture from a
        // region. Quiet ground still has small clusters and colored shadows.
        const double density = world       ? .58 + broad * .30
                               : cityGrass ? .92 + broad * .08
                                           : .72 + broad * .28;
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
        const double scale = cityGrass ? 12. : 16.;
        const int px = int(std::floor(x * scale + warpX)),
                  py = int(std::floor(y * scale + warpY));
        const int xx =
            (px % art.materialWidth + art.materialWidth) % art.materialWidth;
        const int yy =
            (py % art.materialHeight + art.materialHeight) % art.materialHeight;
        const auto color =
            (*art.materialPixels)[std::size_t(yy) * art.materialWidth + xx];
        if (cityGrass)
        {
            const auto sample = [&](int x, int y)
            {
                return (
                    *art.materialPixels
                )[std::size_t((y + art.materialHeight) % art.materialHeight) *
                      art.materialWidth +
                  (x + art.materialWidth) % art.materialWidth];
            };
            const RenderColor neighbors[]{
                sample(xx - 1, yy),
                sample(xx + 1, yy),
                sample(xx, yy - 1),
                sample(xx, yy + 1)
            };
            bool connected = false;
            for (auto n : neighbors)
            {
                connected |= samePaint(color, n);
            }
            // Remove isolated contrasting pinpricks; retain connected authored
            // blades and spread their clusters through continuous coordinates.
            if (!connected)
            {
                for (auto n : neighbors)
                {
                    int count = 0;
                    for (auto other : neighbors)
                    {
                        count += samePaint(n, other);
                    }
                    if (count >= 3)
                    {
                        return n;
                    }
                }
            }
        }
        return color;
    }
} // namespace Paladin
