#pragma once

namespace Paladin
{
    struct TileRenderMetrics
    {
        // Base display size of one logical tile before camera zoom.
        //
        // This is PRESENTATION ONLY.
        // WorldGrid and WorldTile never know this value exists.
        double tilePixels = 4.0;

        [[nodiscard]]
        double scaledTilePixels(
            double cameraZoom
        ) const noexcept
        {
            return tilePixels * cameraZoom;
        }
    };
}