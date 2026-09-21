#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/SceneDetail.h"
#include "rendering/ScenePresentation.h"
#include "rendering/TransportPresentation.h"
#include "world/entities/animals/SettlementAnimals.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementEmploymentState.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include "ui/BitmapFontRenderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Paladin
{
    void SettlementCitizenRenderer::render(
        Renderer& renderer,
        const SettlementCitizenState& citizens,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SettlementAnimals* animals,
        double interpolationAlpha,
        SceneDrawQueue* shared,
        const SceneSpriteLibrary* sprites,
        const CityPresentation* policy,
        const SettlementEmploymentState* employment,
        CitizenId selectedCitizen
    ) const
    {
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        const float adultMarkerSize = static_cast<float>(tilePixels * 0.5);
        auto& queue = shared ? *shared : drawQueue_;
        if (!shared)
        {
            queue.clear();
        }
        const SceneProjection projection{
            camera.tileX(),
            camera.tileY(),
            tilePixels,
            renderer.outputWidth(),
            renderer.outputHeight()
        };
        animationSeconds_ = sprites ? sprites->time() : 0;
        auto& sleeping = sleeping_;
        sleeping.clear();
        auto& fishing = fishing_;
        fishing.clear();

        for (const SettlementCitizen& citizen : citizens.citizens())
        {
            if (citizen.militaryDeployed ||
                citizen.activity == CitizenActivity::UndergroundMining)
            {
                continue;
            }
            const double sleepOffset =
                citizen.activity == CitizenActivity::Sleeping &&
                        citizen.insideHome
                    ? citizen.bedVisualOffsetX
                    : 0;
            const float markerSize =
                adultMarkerSize * (citizen.child ? .5F : 1.0F);
            const double centerX =
                static_cast<double>(renderer.outputWidth()) * 0.5 +
                (citizen.renderX(citizen.visualX(), interpolationAlpha) +
                 sleepOffset + 0.5 - camera.tileX()) *
                    tilePixels;
            const double centerY =
                static_cast<double>(renderer.outputHeight()) * 0.5 +
                (citizen.renderY(citizen.visualY(), interpolationAlpha) + 0.5 -
                 camera.tileY()) *
                    tilePixels;

            if (centerX + markerSize < 0.0 || centerY + markerSize < 0.0 ||
                centerX - markerSize > renderer.outputWidth() ||
                centerY - markerSize > renderer.outputHeight())
            {
                continue;
            }

            // Presentation follows employment without changing citizen
            // simulation.
            const char* role = "citizen";
            if (!citizen.child && employment)
            {
                if (const auto* work =
                        employment->workplace(citizen.workplaceId))
                {
                    const auto& type = work->objectTypeId;
                    if (work->constructionId)
                    {
                        role = "builder";
                    }
                    else if (type == "wheat_farm")
                    {
                        role = "farmer";
                    }
                    else if (type == "fishing_grounds")
                    {
                        role = "fisher";
                    }
                    else if (type == "logging_grounds")
                    {
                        role = "logger";
                    }
                    else if (type == "pastureland")
                    {
                        role = "herder";
                    }
                    else if (type == "bakery")
                    {
                        role = "baker";
                    }
                    else if (
                        type == "coal_mine" || type == "iron_mine" ||
                        type == "gold_mine" || type == "quarry"
                    )
                    {
                        role = "miner";
                    }
                    else if (type == "market")
                    {
                        role = "merchant";
                    }
                    else if (type == "stockpile" || type == "trade_depot")
                    {
                        role = "porter";
                    }
                    else if (type == "barracks")
                    {
                        role = "militia";
                    }
                    else if (type == "army_supply_depot")
                    {
                        role = "porter";
                    }
                }
            }
            if (!citizen.child &&
                citizen.activity == CitizenActivity::Constructing)
            {
                role = "builder";
            }
            if (!citizen.child && citizen.task.kind == CitizenTaskKind::Gather)
            {
                role = "logger";
            }
            if (citizen.soldierId)
            {
                role = "militia";
            }
            bool north = false;
            if (citizen.pathIndex < citizen.path.size())
            {
                north = citizen.path[citizen.pathIndex].y < citizen.visualY();
            }
            std::string spriteId =
                std::string("citizen.") + role +
                (citizen.sex == CitizenSex::Female ? ".female." : ".male.") +
                (north ? "back" : "front");
            const bool walking = citizen.pathIndex < citizen.path.size();
            const bool gathering =
                citizen.task.kind == CitizenTaskKind::Gather && !walking;
            const bool working =
                !walking &&
                (gathering || citizen.task.kind == CitizenTaskKind::Build ||
                 citizen.activity == CitizenActivity::Mining);
            const int pose =
                walking ? int(std::fmod(citizen.walkDistance, 1.0) * 4.0)
                : working
                    ? int(std::fmod(citizen.workAnimationMinutes, 8.0) * .5)
                    : 0;
            if ((walking || working) && sprites &&
                sprites->find(spriteId + ".walk"))
            {
                spriteId += ".walk";
            }
            const std::uint64_t actorKey =
                citizen.soldierId
                    ? (std::uint64_t(3) << 61) | citizen.soldierId.value()
                    : (std::uint64_t(1) << 62) | citizen.id.value();
            if (citizen.inFishingBoat && sprites)
            {
                const auto next = citizen.pathIndex < citizen.path.size()
                                      ? citizen.path[citizen.pathIndex]
                                      : citizen.tilePosition;
                const double dx = next.x - citizen.visualX();
                const double dy = next.y - citizen.visualY();
                if (const auto* boat = sprites->find(
                        std::string("transport.canoe.") +
                        transportDirection(dx, dy)
                    ))
                {
                    const double x =
                        citizen.renderX(citizen.visualX(), interpolationAlpha) +
                        .5;
                    const double y =
                        citizen.renderY(citizen.visualY(), interpolationAlpha) +
                        .5;
                    queue.submit(
                        {projection.bounds(
                             {x,
                              y,
                              0,
                              boat->width * .8,
                              boat->height * .8,
                              boat->pivotX,
                              boat->pivotY}
                         ),
                         {},
                         y,
                         actorKey,
                         0,
                         -5,
                         boat->texture.get(),
                         sprites->frame(*boat)}
                    );
                }
            }
            const bool custom =
                (tilePixels >= StaticDetailPixels ||
                 citizen.id == selectedCitizen) &&
                sprites &&
                sprites->submit(
                    queue,
                    projection,
                    sprites->find(spriteId) ? spriteId : "citizen",
                    citizen.renderX(citizen.visualX(), interpolationAlpha) +
                        sleepOffset + .5,
                    citizen.renderY(citizen.visualY(), interpolationAlpha) + .5,
                    actorKey,
                    citizen.child           ? .5
                    : citizen.inFishingBoat ? .75
                                            : 1,
                    citizen.inFishingBoat ? 0 : pose
                );
            if (working && tilePixels >= AnimationDetailPixels)
            {
                // Tools and impact flecks share the 16-art-pixel city raster.
                // Quantized poses follow actual work time; pause freezes them.
                const float p = float(tilePixels / 16.0);
                const float x = float(centerX), y = float(centerY);
                const int tipX[4] = {6, 9, 9, 7}, tipY[4] = {-9, -6, -1, -4};
                const auto pixel = [&](float px,
                                       float py,
                                       int w,
                                       int h,
                                       RenderColor color,
                                       int part)
                {
                    queue.submit(
                        {{x + px * p, y + py * p, w * p, h * p},
                         color,
                         citizen.renderY(
                             citizen.visualY(),
                             interpolationAlpha
                         ) + .5,
                         actorKey,
                         0,
                         part}
                    );
                };
                const int tx = tipX[pose], ty = tipY[pose];
                const int steps = std::max(std::abs(tx - 3), std::abs(ty + 3));
                for (int i = 0; i <= steps; ++i)
                {
                    pixel(
                        std::round(
                            3 + (tx - 3) * float(i) / std::max(1, steps)
                        ),
                        std::round(
                            -3 + (ty + 3) * float(i) / std::max(1, steps)
                        ),
                        1,
                        1,
                        {136, 96, 68, 255},
                        2
                    );
                }
                if (citizen.activity == CitizenActivity::Mining)
                {
                    pixel(
                        float(tx - 2),
                        float(ty - 1),
                        5,
                        1,
                        {154, 167, 175, 255},
                        3
                    );
                    pixel(
                        float(tx - 3),
                        float(ty),
                        1,
                        1,
                        {108, 116, 122, 255},
                        3
                    );
                    pixel(
                        float(tx + 3),
                        float(ty),
                        1,
                        1,
                        {108, 116, 122, 255},
                        3
                    );
                }
                else
                {
                    pixel(
                        float(tx - 1),
                        float(ty - 1),
                        3,
                        2,
                        {154, 167, 175, 255},
                        3
                    );
                }
                pixel(
                    float(tx - 1),
                    float(ty - 1),
                    2,
                    1,
                    {215, 224, 227, 255},
                    4
                );
                if (pose == 2)
                {
                    pixel(10, 0, 1, 1, {213, 164, 84, 255}, 5);
                    pixel(12, -2, 1, 1, {167, 141, 114, 255}, 5);
                    pixel(10, -4, 1, 1, {235, 196, 107, 255}, 5);
                }
            }
            if (tilePixels >= AnimationDetailPixels &&
                citizen.activity == CitizenActivity::Sleeping)
            {
                sleeping.emplace_back(
                    float(centerX) - markerSize * .5F,
                    float(centerY) - markerSize * .5F
                );
            }
            if (tilePixels >= AnimationDetailPixels &&
                citizen.activity == CitizenActivity::Fishing &&
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
            if (!custom && tilePixels >= StaticDetailPixels && policy &&
                policy->shadowsVisible)
            {
                queue.submit(
                    {{float(centerX) - markerSize * .5F,
                      float(centerY) + markerSize * .3F,
                      markerSize,
                      markerSize * .3F},
                     policy->shadowColor,
                     0,
                     (std::uint64_t(1) << 62) | citizen.id.value(),
                     -1}
                );
            }
            if (!custom)
            {
                queue.submit(
                    {{static_cast<float>(centerX) - markerSize * 0.5F,
                      static_cast<float>(centerY) - markerSize * 0.5F,
                      markerSize,
                      markerSize},
                     CitizenPlaceholderColor,
                     citizen.renderY(citizen.visualY(), interpolationAlpha) +
                         .5,
                     (std::uint64_t(1) << 62) | citizen.id.value()}
                );
            }
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
                const auto* leader = animal.beingLed
                                         ? citizens.citizen(animal.handler)
                                         : nullptr;
                // Use the escort's continuous movement, not a fresh one-minute
                // animal tween restarted on every (possibly faster) road step.
                const double groundX =
                    leader
                        ? leader->renderX(
                              leader->visualX(),
                              interpolationAlpha
                          ) + .72
                        : animal.renderX(animal.visualX(), interpolationAlpha);
                const double groundY =
                    leader
                        ? leader->renderY(
                              leader->visualY(),
                              interpolationAlpha
                          ) + .28
                        : animal.renderY(animal.visualY(), interpolationAlpha);
                const SceneVisual visual{
                    groundX + .5,
                    groundY + .5,
                    0,
                    d->markerWidth * size,
                    d->markerHeight * size,
                    .5,
                    .5
                };
                const bool custom =
                    sprites && sprites->submit(
                                   queue,
                                   projection,
                                   "animal." + animal.species,
                                   visual.groundX,
                                   visual.groundY,
                                   (std::uint64_t(2) << 62) | animal.id.value(),
                                   size
                               );
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
                         (std::uint64_t(2) << 62) | animal.id.value(),
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
                if (tilePixels >= StaticDetailPixels && policy &&
                    policy->shadowsVisible)
                {
                    queue.submit(
                        {{body.x,
                          body.y + body.height * .8F,
                          body.width,
                          body.height * .3F},
                         policy->shadowColor,
                         0,
                         (std::uint64_t(2) << 62) | animal.id.value(),
                         -1}
                    );
                }
                if (custom)
                {
                    continue;
                }
                part(0, 0, 1, 1, coat, 1);
                if (tilePixels >= StaticDetailPixels)
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
        if (!shared)
        {
            queue.render(renderer);
            renderAnnotations(renderer, tilePixels);
        }
    }
    void SettlementCitizenRenderer::renderAnnotations(
        Renderer& renderer,
        double tilePixels
    ) const
    {
        const float adultMarkerSize = float(tilePixels * .5);
        for (const auto& line : fishing_)
        {
            renderer.drawLine(
                line.x,
                line.y,
                line.x + line.dx * .7F,
                line.y + line.dy * .7F - adultMarkerSize,
                {136, 96, 68, 255}
            );
            renderer.drawLine(
                line.x + line.dx * .7F,
                line.y + line.dy * .7F - adultMarkerSize,
                line.x + line.dx,
                line.y + line.dy,
                {215, 224, 227, 255}
            );
        }
        // Three authored 3x5 Zs, all one city-art-texel strokes. No font
        // resampling or native-screen bypass. The silhouette stays readable at
        // the closest zoom and rises in whole-art-pixel steps with world time.
        const float p = float(tilePixels / 16.0);
        constexpr unsigned glyph[5] = {7, 1, 2, 4, 7};
        for (const auto& [x, y] : sleeping_)
        {
            for (int z = 0; z < 3; ++z)
            {
                const int lift =
                    (int(std::fmod(animationSeconds_, 4.0) * .75) + z) % 3;
                const float ox = x + float(z * 5 - 3) * p,
                            oy = y - float(7 + z * 3 + lift) * p;
                for (int row = 0; row < 5; ++row)
                {
                    for (int col = 0; col < 3; ++col)
                    {
                        if (glyph[row] & (1u << (2 - col)))
                        {
                            renderer.fillRectangle(
                                ox + (col + 1) * p,
                                oy + (row + 1) * p,
                                p,
                                p,
                                {8, 15, 27, 255}
                            );
                        }
                    }
                }
                for (int row = 0; row < 5; ++row)
                {
                    for (int col = 0; col < 3; ++col)
                    {
                        if (glyph[row] & (1u << (2 - col)))
                        {
                            renderer.fillRectangle(
                                ox + col * p,
                                oy + row * p,
                                p,
                                p,
                                {175, 201, 214, 255}
                            );
                        }
                    }
                }
            }
        }
    }
} // namespace Paladin
