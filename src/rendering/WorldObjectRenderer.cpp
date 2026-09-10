#include "rendering/WorldObjectRenderer.h"

#include "rendering/GlobeView.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "world/Army.h"
#include "world/Realm.h"
#include "world/Settlement.h"
#include "world/World.h"
#include "world/WorldRoad.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace Paladin
{
    namespace
    {
        struct ProjectedWorldObject
        {
            float x = 0.0F;
            float y = 0.0F;
            float visibility = 1.0F;
            double depth = 1.0;
        };

        RenderColor worldObjectColor(
            const World& world,
            RealmId owner,
            RenderColor fallback
        ) noexcept
        {
            if (const Realm* realm = world.realm(owner))
            {
                const MapColor color = realm->mapColor();
                return {color.red, color.green, color.blue, 255};
            }
            return fallback;
        }

        RenderColor visibleColor(RenderColor color, float visibility) noexcept
        {
            const float weight = std::clamp(visibility, 0.0F, 1.0F);
            color.alpha = static_cast<std::uint8_t>(std::clamp(
                std::lround(double(color.alpha) * weight),
                0L,
                255L
            ));
            return color;
        }

        bool outside(
            const ProjectedWorldObject& point,
            const Renderer& renderer,
            float margin
        ) noexcept
        {
            return point.x < -margin || point.y < -margin ||
                   point.x > renderer.outputWidth() + margin ||
                   point.y > renderer.outputHeight() + margin;
        }
    } // namespace

    void WorldObjectRenderer::render(
        Renderer& renderer,
        const World& world,
        const Camera2D& renderCamera,
        double effectiveTilePixels,
        bool globe,
        const WorldPresentationState& presentation,
        std::span<const SpriteRenderItem> fallbackSprites,
        std::optional<WorldPlacementMarker> placementMarker
    ) const
    {
        if (renderer.outputWidth() <= 0 || renderer.outputHeight() <= 0 ||
            world.grid().width() <= 0 || world.grid().height() <= 0 ||
            !std::isfinite(effectiveTilePixels) || effectiveTilePixels <= 0.0)
        {
            return;
        }

        const float localWeight = globe
                                      ? std::clamp(
                                            presentation.localWorldWeight,
                                            0.0F,
                                            1.0F
                                        )
                                      : 0.0F;
        const float worldObjectVisibility = std::clamp(
            presentation.regionalWeight + presentation.localWorldWeight,
            0.0F,
            1.0F
        );

        const auto globeView = GlobeView::from(
            renderCamera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight()
        );
        const auto tangentView = LocalTangentWorldView::from(
            renderCamera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight(),
            effectiveTilePixels
        );

        const auto project = [&](double tileX, double tileY)
            -> std::optional<ProjectedWorldObject>
        {
            if (!globe)
            {
                return ProjectedWorldObject{
                    float(
                        renderer.outputWidth() * 0.5 +
                        (tileX - renderCamera.tileX()) * effectiveTilePixels
                    ),
                    float(
                        renderer.outputHeight() * 0.5 +
                        (tileY - renderCamera.tileY()) * effectiveTilePixels
                    ),
                    1.0F,
                    1.0
                };
            }

            const auto sphere = globeView.project(
                tileX / world.grid().width(),
                tileY / world.grid().height()
            );
            const auto tangent = tangentView.projectTiles(tileX, tileY);
            if (!std::isfinite(sphere.x) || !std::isfinite(sphere.y) ||
                !std::isfinite(sphere.z))
            {
                return std::nullopt;
            }
            if (sphere.z <= 0.0 && localWeight < 0.5F)
            {
                return std::nullopt;
            }

            const float sphereVisibility =
                std::clamp(float(sphere.z * 4.0), 0.0F, 1.0F);
            return ProjectedWorldObject{
                float(std::lerp(sphere.x, tangent.x, double(localWeight))),
                float(std::lerp(sphere.y, tangent.y, double(localWeight))),
                std::lerp(sphereVisibility, 1.0F, localWeight),
                std::lerp(sphere.z, 1.0, double(localWeight))
            };
        };

        // Everything below this point shares exactly the same 32-art-pixel
        // lattice. At close zoom this is a transparent higher-resolution layer
        // above the 16-pixel terrain scene, so strategic symbols do not inherit
        // terrain's chunky raster or its sampling phase.
        WorldObjectPixelScene objectScene(renderer, effectiveTilePixels);

        // Persistent strategic roads live below the point objects. They are not
        // settlement roads and have no effect on settlement navigation yet.
        if (worldObjectVisibility > 0.001F)
        {
            for (const WorldRoad& road : world.worldRoads())
            {
                const auto points = road.points();
                if (points.size() < 2)
                {
                    continue;
                }
                const RenderColor roadColor = visibleColor(
                    worldObjectColor(
                        world,
                        road.ownerRealmId(),
                        {116, 81, 63, 255}
                    ),
                    worldObjectVisibility
                );
                const RenderColor shadow = visibleColor(
                    {8, 15, 27, 220},
                    worldObjectVisibility
                );
                for (std::size_t index = 1; index < points.size(); ++index)
                {
                    const auto a = project(
                        double(points[index - 1].x) + 0.5,
                        double(points[index - 1].y) + 0.5
                    );
                    const auto b = project(
                        double(points[index].x) + 0.5,
                        double(points[index].y) + 0.5
                    );
                    if (!a || !b ||
                        (outside(*a, renderer, 64.0F) &&
                         outside(*b, renderer, 64.0F)))
                    {
                        continue;
                    }
                    const double dx = double(b->x) - a->x;
                    const double dy = double(b->y) - a->y;
                    const double length = std::hypot(dx, dy);
                    if (length <= 0.001)
                    {
                        continue;
                    }
                    const float nx = float(-dy / length);
                    const float ny = float(dx / length);
                    renderer.drawLine(
                        a->x + nx,
                        a->y + ny,
                        b->x + nx,
                        b->y + ny,
                        shadow
                    );
                    renderer.drawLine(
                        a->x - nx,
                        a->y - ny,
                        b->x - nx,
                        b->y - ny,
                        shadow
                    );
                    renderer.drawLine(a->x, a->y, b->x, b->y, roadColor);
                }
            }
        }

        if (worldObjectVisibility > 0.001F)
        {
            struct SettlementProjection
            {
                const Settlement* settlement = nullptr;
                ProjectedWorldObject point;
            };
            std::vector<SettlementProjection> settlements;
            settlements.reserve(world.settlements().size());
            for (const Settlement& settlement : world.settlements())
            {
                const auto position = settlement.position();
                auto point = project(
                    double(position.x) + 0.5,
                    double(position.y) + 0.5
                );
                if (!point || outside(*point, renderer, 220.0F))
                {
                    continue;
                }
                point->visibility *= worldObjectVisibility;
                settlements.push_back({&settlement, *point});
            }
            std::stable_sort(
                settlements.begin(),
                settlements.end(),
                [](const auto& a, const auto& b)
                { return a.point.depth < b.point.depth; }
            );
            for (const auto& item : settlements)
            {
                // Regional semantics call this the settlement marker. In the
                // close band the same established rendering is temporarily the
                // Settlement world-object fallback. Authored settlement sprites
                // can replace only that close slot later without changing the
                // simulation entity.
                settlementMarkerRenderer_.drawAt(
                    renderer,
                    world,
                    *item.settlement,
                    item.point.x,
                    item.point.y,
                    item.point.visibility
                );
            }

            for (const Army& army : world.armies())
            {
                const auto position = army.position();
                const auto point = project(
                    double(position.x) + 0.5,
                    double(position.y) + 0.5
                );
                if (!point || outside(*point, renderer, 64.0F))
                {
                    continue;
                }
                const float visibility =
                    worldObjectVisibility * point->visibility;
                const float radius = std::clamp(
                    float(effectiveTilePixels * 0.22),
                    5.0F,
                    11.0F
                );
                const RenderColor shadow =
                    visibleColor({8, 15, 27, 230}, visibility);
                const RenderColor banner = visibleColor(
                    worldObjectColor(
                        world,
                        army.ownerRealmId(),
                        {255, 215, 131, 255}
                    ),
                    visibility
                );
                const RenderColor tip =
                    visibleColor({244, 243, 232, 255}, visibility);

                // Temporary no-sprite army presentation. It deliberately uses
                // the same object grid as every other world object.
                renderer.fillRectangle(
                    point->x - radius - 1.0F,
                    point->y - radius - 1.0F,
                    radius * 2.0F + 2.0F,
                    radius * 2.0F + 2.0F,
                    shadow
                );
                renderer.fillRectangle(
                    point->x - radius,
                    point->y - radius,
                    radius * 2.0F,
                    radius * 2.0F,
                    banner
                );
                renderer.fillRectangle(
                    point->x - 1.0F,
                    point->y - radius + 1.0F,
                    2.0F,
                    radius * 2.0F - 2.0F,
                    tip
                );
            }

            for (const SpriteRenderItem& item : fallbackSprites)
            {
                if (!item.texture)
                {
                    continue;
                }
                const auto point = project(item.tileX, item.tileY);
                if (!point || outside(*point, renderer, 256.0F))
                {
                    continue;
                }
                const float sourceWidth = item.sourceWidth > 0.0F
                                              ? item.sourceWidth
                                              : float(item.texture->width());
                const float sourceHeight = item.sourceHeight > 0.0F
                                               ? item.sourceHeight
                                               : float(item.texture->height());
                const float width =
                    (item.displayWidthPixels > 0.0F ? item.displayWidthPixels
                                                    : sourceWidth) *
                    float(renderCamera.zoom());
                const float height =
                    (item.displayHeightPixels > 0.0F ? item.displayHeightPixels
                                                     : sourceHeight) *
                    float(renderCamera.zoom());
                renderer.drawTexture(
                    *item.texture,
                    item.sourceX,
                    item.sourceY,
                    sourceWidth,
                    sourceHeight,
                    point->x - width * item.anchorX,
                    point->y - height * item.anchorY,
                    width,
                    height,
                    static_cast<std::uint8_t>(std::clamp(
                        std::lround(
                            255.0 * worldObjectVisibility * point->visibility
                        ),
                        0L,
                        255L
                    ))
                );
            }
        }

        if (placementMarker)
        {
            const auto point = project(
                double(placementMarker->position.x) + 0.5,
                double(placementMarker->position.y) + 0.5
            );
            if (point && !outside(*point, renderer, 64.0F))
            {
                RenderColor color = visibleColor(
                    placementMarker->color,
                    point->visibility
                );
                RenderColor shadow = visibleColor(
                    {8, 15, 27, 220},
                    point->visibility
                );
                const float radius = std::clamp(
                    float(effectiveTilePixels * 0.24),
                    5.0F,
                    10.0F
                );
                renderer.drawLine(
                    point->x,
                    point->y - radius - 1.0F,
                    point->x + radius + 1.0F,
                    point->y,
                    shadow
                );
                renderer.drawLine(
                    point->x + radius + 1.0F,
                    point->y,
                    point->x,
                    point->y + radius + 1.0F,
                    shadow
                );
                renderer.drawLine(
                    point->x,
                    point->y + radius + 1.0F,
                    point->x - radius - 1.0F,
                    point->y,
                    shadow
                );
                renderer.drawLine(
                    point->x - radius - 1.0F,
                    point->y,
                    point->x,
                    point->y - radius - 1.0F,
                    shadow
                );
                renderer.drawLine(
                    point->x,
                    point->y - radius,
                    point->x + radius,
                    point->y,
                    color
                );
                renderer.drawLine(
                    point->x + radius,
                    point->y,
                    point->x,
                    point->y + radius,
                    color
                );
                renderer.drawLine(
                    point->x,
                    point->y + radius,
                    point->x - radius,
                    point->y,
                    color
                );
                renderer.drawLine(
                    point->x - radius,
                    point->y,
                    point->x,
                    point->y - radius,
                    color
                );
                renderer.fillRectangle(
                    point->x - 1.0F,
                    point->y - 1.0F,
                    3.0F,
                    3.0F,
                    color
                );
            }
        }
    }
} // namespace Paladin
