#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/ScenePresentation.h"
#include "world/entities/animals/SettlementAnimals.h"

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
        const TileRenderMetrics& metrics,
        const SettlementAnimals* animals
    ) const
    {
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        const float adultMarkerSize = static_cast<float>(tilePixels * 0.5);
        auto& queue = drawQueue_;
        queue.clear();
        const SceneProjection projection{
            camera.tileX(),
            camera.tileY(),
            tilePixels,
            renderer.outputWidth(),
            renderer.outputHeight()
        };
        std::vector<std::pair<float, float>> sleeping;
        struct FishingLine
        {
            float x, y, dx, dy;
        };
        std::vector<FishingLine> fishing;

        for (const SettlementCitizen& citizen : citizens.citizens())
        {
            const float markerSize =
                adultMarkerSize * (citizen.child ? .5F : 1.0F);
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
            queue.submit(
                {{static_cast<float>(centerX) - markerSize * 0.5F,
                  static_cast<float>(centerY) - markerSize * 0.5F,
                  markerSize,
                  markerSize},
                 {210, 180, 140, 255},
                 citizen.visualY() + .5,
                 citizen.id.value()}
            );
        }

        if (animals)
        {
            for (const auto& animal : animals->all())
            {
                if (animal.health <= 0)
                {
                    continue;
                }
                const auto* d = animalSpecies(animal.species);
                if (!d)
                {
                    continue;
                }
                const double size = animal.juvenile ? .65 : 1;
                const SceneVisual visual{
                    animal.visualX() + .5,
                    animal.visualY() + .5,
                    0,
                    d->markerWidth * size,
                    d->markerHeight * size,
                    .5,
                    .5
                };
                const auto body = projection.bounds(visual);
                if (!projection.visible(body))
                {
                    continue;
                }
                const auto part = [&](float x,
                                      float y,
                                      float w,
                                      float h,
                                      RenderColor color,
                                      int order)
                {
                    queue.submit(
                        {{body.x + body.width * x,
                          body.y + body.height * y,
                          body.width * w,
                          body.height * h},
                         color,
                         visual.groundY,
                         animal.id.value(),
                         0,
                         order}
                    );
                };
                const RenderColor coat{
                    std::uint8_t(d->coatRgb >> 16),
                    std::uint8_t(d->coatRgb >> 8),
                    std::uint8_t(d->coatRgb),
                    255
                };
                if (animal.order != AnimalOrder::None)
                {
                    part(
                        -.08F,
                        -.12F,
                        1.16F,
                        1.24F,
                        animal.order == AnimalOrder::Hunt
                            ? RenderColor{221, 75, 67, 255}
                            : RenderColor{229, 196, 70, 255},
                        0
                    );
                }
                part(0, 0, 1, 1, coat, 1);
                if (tilePixels >= 5)
                {
                    part(.10F, .8F, .13F, .25F, {70, 52, 39, 255}, 2);
                    part(.66F, .8F, .13F, .25F, {70, 52, 39, 255}, 3);
                    part(.74F, -.1F, .35F, .55F, coat, 4);
                    if (animal.species == "cow")
                    {
                        part(.23F, .15F, .3F, .48F, {81, 55, 37, 255}, 5);
                    }
                    if (animal.species == "pig")
                    {
                        part(.95F, .05F, .17F, .30F, {175, 94, 110, 255}, 5);
                    }
                    if (animal.species == "chicken")
                    {
                        part(.73F, -.22F, .2F, .3F, {188, 53, 44, 255}, 5);
                        part(1, -.02F, .24F, .15F, {226, 166, 45, 255}, 6);
                    }
                    part(.94F, .03F, .055F, .08F, {30, 27, 24, 255}, 7);
                }
            }
        }
        queue.render(renderer);
        for (const auto& line : fishing)
        {
            renderer.drawLine(
                line.x,
                line.y,
                line.x + line.dx * .7F,
                line.y + line.dy * .7F - adultMarkerSize,
                {123, 88, 54, 255}
            );
            renderer.drawLine(
                line.x + line.dx * .7F,
                line.y + line.dy * .7F - adultMarkerSize,
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
