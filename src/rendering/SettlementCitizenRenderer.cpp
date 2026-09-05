#include "rendering/SettlementCitizenRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include "ui/BitmapFontRenderer.h"
#include <algorithm>
#include <vector>

namespace Paladin
{
void SettlementCitizenRenderer::render(
    Renderer& renderer,
    const SettlementCitizenState& citizens,
    const Camera2D& camera,
    const TileRenderMetrics& metrics
) const
{
    const double tilePixels = metrics.scaledTilePixels(camera.zoom());
    const float markerSize = static_cast<float>(tilePixels * 0.5);
    std::vector<RenderRectangle> rectangles;
    rectangles.reserve(citizens.citizens().size());
    std::vector<std::pair<float, float>> sleeping;
    struct FishingLine
    {
        float x, y, dx, dy;
    };
    std::vector<FishingLine> fishing;

    for (const SettlementCitizen& citizen : citizens.citizens())
    {
        const double centerX =
            static_cast<double>(renderer.outputWidth()) * 0.5 +
            (citizen.visualX() + 0.5 - camera.tileX()) * tilePixels;
        const double centerY =
            static_cast<double>(renderer.outputHeight()) * 0.5 +
            (citizen.visualY() + 0.5 - camera.tileY()) * tilePixels;

        if (centerX + markerSize < 0.0 || centerY + markerSize < 0.0 ||
            centerX - markerSize > renderer.outputWidth() ||
            centerY - markerSize > renderer.outputHeight())
        {
            continue;
        }

        if (citizen.activity == CitizenActivity::Sleeping)
        {
            sleeping.emplace_back(
                float(centerX) - markerSize * .5F,
                float(centerY) - markerSize * .5F
            );
        }
        if (citizen.activity == CitizenActivity::Fishing &&
            citizen.path.empty())
        {
            fishing.push_back(
                {float(centerX),
                 float(centerY),
                 float(citizen.task.target.x - citizen.tilePosition.x) *
                     float(tilePixels) * .65F,
                 float(citizen.task.target.y - citizen.tilePosition.y) *
                     float(tilePixels) * .65F}
            );
        }
        rectangles.push_back(
            {static_cast<float>(centerX) - markerSize * 0.5F,
             static_cast<float>(centerY) - markerSize * 0.5F,
             markerSize,
             markerSize}
        );
    }

    renderer.fillRectangles(rectangles, {210, 180, 140, 255});
    for (const auto& line : fishing)
    {
        renderer.drawLine(
            line.x,
            line.y,
            line.x + line.dx * .7F,
            line.y + line.dy * .7F - markerSize,
            {123, 88, 54, 255}
        );
        renderer.drawLine(
            line.x + line.dx * .7F,
            line.y + line.dy * .7F - markerSize,
            line.x + line.dx,
            line.y + line.dy,
            {209, 216, 222, 255}
        );
    }
    const BitmapFontRenderer font;
    const float scale = std::clamp(float(tilePixels) / 24, 1.0F, 2.0F);
    for (const auto& [x, y] : sleeping)
    {
        font.drawText(
            renderer,
            "z",
            x - 17 * scale,
            y - 7 * scale,
            scale * .75F,
            {103, 185, 248, 255}
        );
        font.drawText(
            renderer,
            "ZZ",
            x - 12 * scale,
            y - 10 * scale,
            scale,
            {103, 185, 248, 255}
        );
    }
}
} // namespace Paladin
