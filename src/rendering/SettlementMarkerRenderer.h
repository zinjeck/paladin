#pragma once

#include "ui/BitmapFontRenderer.h"

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    struct TileRenderMetrics;

    class SettlementMarkerRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        ) const;

    private:
        BitmapFontRenderer fontRenderer_;
    };
}
