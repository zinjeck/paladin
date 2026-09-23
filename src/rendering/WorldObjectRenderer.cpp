#include "rendering/WorldObjectRenderer.h"
#include "rendering/TransportPresentation.h"
#include "rendering/WorldCaravanPresentation.h"
#include <array>

#include "rendering/GlobeView.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/Texture.h"
#include "rendering/WorldArmyPresentation.h"
#include "ui/BitmapFontRenderer.h"
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
            color.alpha = static_cast<std::uint8_t>(
                std::clamp(std::lround(double(color.alpha) * weight), 0L, 255L)
            );
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
        bool stabilizePixelPhase,
        std::span<const SpriteRenderItem> fallbackSprites,
        std::optional<WorldPlacementMarker> placementMarker,
        WorldSurface::Point3 rigidResidual,
        const SceneSpriteLibrary* artwork,
        ArmyId selectedArmy,
        ShipmentId selectedCaravan
    ) const
    {
        if (renderer.outputWidth() <= 0 || renderer.outputHeight() <= 0 ||
            world.grid().width() <= 0 || world.grid().height() <= 0 ||
            !std::isfinite(effectiveTilePixels) || effectiveTilePixels <= 0.0)
        {
            return;
        }

        const float armyVisibility = worldArmyVisibility(effectiveTilePixels);
        const float localWeight =
            globe ? std::clamp(presentation.localWorldWeight, 0.0F, 1.0F)
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

        const auto project =
            [&](double tileX,
                double tileY) -> std::optional<ProjectedWorldObject>
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

        // Real world geometry retains its 32 px/tile detail. At close globe
        // scale it is rasterized in the SAME unrotated source chart as terrain,
        // then rotated and translated as one layer. Billboard sprites use the
        // same projected ground anchors but never inherit terrain rotation.
        const bool planarObjects = globe && localWeight >= .999F;
        const double overscan =
            planarObjects ? tangentView.overscanScale(
                                effectiveTilePixels / WorldPixelsPerTile + 4.0
                            )
                          : 1.0;
        const double geometryPixels = effectiveTilePixels / overscan;
        const auto geometryProject =
            [&](double x, double y) -> std::optional<ProjectedWorldObject>
        {
            if (!planarObjects)
            {
                return project(x, y);
            }
            double dx = x - renderCamera.tileX();
            dx -= std::round(dx / world.grid().width()) * world.grid().width();
            return ProjectedWorldObject{
                float(renderer.outputWidth() * .5 + dx * geometryPixels),
                float(
                    renderer.outputHeight() * .5 +
                    (y - renderCamera.tileY()) * geometryPixels
                ),
                1,
                1
            };
        };
        if (worldObjectVisibility > .001F && !world.worldRoads().empty())
        {
            WorldObjectPixelScene objectScene(
                renderer,
                geometryPixels,
                planarObjects
                    ? tangentView.rollRadians() * 180.0 / 3.14159265358979323846
                    : 0.0,
                overscan,
                rigidResidual.x,
                rigidResidual.y
            );
            // Persistent strategic roads live below the point objects. They are
            // not settlement roads and have no effect on settlement navigation
            // yet.
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
                    const RenderColor shadow =
                        visibleColor({8, 15, 27, 220}, worldObjectVisibility);
                    for (std::size_t index = 1; index < points.size(); ++index)
                    {
                        const auto a = geometryProject(
                            double(points[index - 1].x) + 0.5,
                            double(points[index - 1].y) + 0.5
                        );
                        const auto b = geometryProject(
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

        } // Ground-locked roads end their rotated source-chart layer here.

        if ((worldObjectVisibility > .001F &&
             (artwork || !fallbackSprites.empty())) ||
            (artwork && !world.armies().empty()) || !world.shipments().empty())
        {
            // Upright billboards at every latitude, roll and projection/LOD.
            // Only ground anchors follow the globe. Rotating this layer used to
            // turn towns upside down, with a sudden flip at localWeight ==
            // .999.
            WorldObjectPixelScene billboardScene(
                renderer,
                effectiveTilePixels,
                0.0,
                1.0,
                rigidResidual.x,
                rigidResidual.y
            );
            if (worldObjectVisibility > .001F)
            {
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
                    const float sourceWidth =
                        item.sourceWidth > 0.0F ? item.sourceWidth
                                                : float(item.texture->width());
                    const float sourceHeight =
                        item.sourceHeight > 0.0F
                            ? item.sourceHeight
                            : float(item.texture->height());
                    const float width = (item.displayWidthPixels > 0.0F
                                             ? item.displayWidthPixels
                                             : sourceWidth) *
                                        float(renderCamera.zoom());
                    const float height = (item.displayHeightPixels > 0.0F
                                              ? item.displayHeightPixels
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
                                255.0 * worldObjectVisibility *
                                point->visibility
                            ),
                            0L,
                            255L
                        ))
                    );
                }
            }

            // Caravan art is a small covered-wagon pixel glyph in the SAME
            // upright 32-texel object layer, never a native-screen floating
            // badge.
            if (armyVisibility > .001F)
            {
                std::size_t routeBudget = 2048;
                for (const auto& caravan : world.shipments())
                {
                    if (!worldCaravanVisible(caravan, effectiveTilePixels))
                    {
                        continue;
                    }
                    // Real trade direction follows cargo source -> buyer. Limit
                    // route samples regardless of map size or shipment backlog.
                    const auto stride =
                        std::max<std::size_t>(1, caravan.path.size() / 64);
                    const auto* owner = world.realm(caravan.owner);
                    const auto* buyer = world.realm(caravan.buyer);
                    const bool showRoute = caravan.id == selectedCaravan ||
                                           (owner && !owner->aiControlled) ||
                                           (buyer && !buyer->aiControlled);
                    for (std::size_t i = stride;
                         showRoute && i < caravan.path.size() && routeBudget;
                         i += stride)
                    {
                        --routeBudget;
                        const auto a = project(
                            caravan.path[i - stride].x + .5,
                            caravan.path[i - stride].y + .5
                        );
                        const auto b = project(
                            caravan.path[i].x + .5,
                            caravan.path[i].y + .5
                        );
                        if (!a || !b ||
                            (outside(*a, renderer, 20) &&
                             outside(*b, renderer, 20)))
                        {
                            continue;
                        }
                        const double dx = b->x - a->x, dy = b->y - a->y;
                        const double length = std::hypot(dx, dy);
                        if (length > renderer.outputWidth() * .5 || length < 1)
                        {
                            continue;
                        }
                        const RenderColor line{213, 164, 84, 75};
                        renderer.drawLine(a->x, a->y, b->x, b->y, line);
                        renderer.drawLine(a->x + 1, a->y, b->x + 1, b->y, line);
                        if ((i / stride) % 4 == 0 && length > 3)
                        {
                            const float mx = (a->x + b->x) * .5F,
                                        my = (a->y + b->y) * .5F;
                            const float ux = float(dx / length) * 4,
                                        uy = float(dy / length) * 4;
                            renderer.drawLine(
                                mx,
                                my,
                                mx - ux - uy * .6F,
                                my - uy + ux * .6F,
                                {235, 196, 107, 150}
                            );
                            renderer.drawLine(
                                mx,
                                my,
                                mx - ux + uy * .6F,
                                my - uy - ux * .6F,
                                {235, 196, 107, 150}
                            );
                        }
                    }
                    const auto point =
                        project(caravan.visualX() + .5, caravan.visualY() + .5);
                    if (!point || outside(*point, renderer, 80))
                    {
                        continue;
                    }
                    const auto from = caravan.position();
                    const auto to = caravan.moving() && caravan.nextIndex() <
                                                            caravan.path.size()
                                        ? caravan.path[caravan.nextIndex()]
                                        : from;
                    double dx = to.x - from.x;
                    if (std::abs(dx) > world.grid().width() * .5)
                    {
                        dx += dx > 0 ? -world.grid().width()
                                     : world.grid().width();
                    }
                    if (const auto* art =
                            artwork ? artwork->find(
                                          std::string(caravan.transportDomain == TransportDomain::Water
                                              ? "transport.boat." : "transport.cart.") +
                                          transportDirection(dx, to.y - from.y)
                                      )
                                    : nullptr)
                    {
                        const auto source = artwork->frame(*art);
                        const float height =
                            worldCaravanPixelStep(effectiveTilePixels) * 24;
                        const float width =
                            height * source.width / source.height;
                        if (caravan.id == selectedCaravan)
                        {
                            const auto color = RenderColor{235, 196, 107, 210};
                            const float left = point->x - width * .5F,
                                        top = point->y - height * .75F;
                            renderer
                                .drawLine(left, top, left + width, top, color);
                            renderer.drawLine(
                                left,
                                top + height,
                                left + width,
                                top + height,
                                color
                            );
                            renderer
                                .drawLine(left, top, left, top + height, color);
                            renderer.drawLine(
                                left + width,
                                top,
                                left + width,
                                top + height,
                                color
                            );
                        }
                        renderer.drawTexture(
                            *art->texture,
                            source.x,
                            source.y,
                            source.width,
                            source.height,
                            point->x - width * .5F,
                            point->y - height * .75F,
                            width,
                            height,
                            std::uint8_t(
                                255 * point->visibility * armyVisibility
                            )
                        );
                        continue;
                    }
                    // A future water shipment must never masquerade as a cart
                    // if its directional boat artwork is absent.
                    if (caravan.transportDomain == TransportDomain::Water) continue;
                    const auto& rows = WorldCaravanRows;
                    const float step =
                        worldCaravanPixelStep(effectiveTilePixels);
                    if (caravan.id == selectedCaravan)
                    {
                        for (int y = 0; y < int(rows.size()); ++y)
                        {
                            for (int x = 0; rows[y][x]; ++x)
                            {
                                if (rows[y][x] != ' ')
                                {
                                    for (int dy = -1; dy <= 1; ++dy)
                                    {
                                        for (int dx = -1; dx <= 1; ++dx)
                                        {
                                            if (dx || dy)
                                            {
                                                renderer.fillRectangle(
                                                    point->x +
                                                        (x - 8 + dx) * step,
                                                    point->y +
                                                        (y - 13 + dy) * step,
                                                    step,
                                                    step,
                                                    visibleColor(
                                                        {235, 196, 107, 255},
                                                        point->visibility *
                                                            armyVisibility
                                                    )
                                                );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for (int y = 0; y < int(rows.size()); ++y)
                    {
                        for (int x = 0; rows[y][x]; ++x)
                        {
                            const char c = rows[y][x];
                            if (c == ' ')
                            {
                                continue;
                            }
                            const RenderColor color =
                                c == 'i'   ? RenderColor{244, 231, 199, 255}
                                : c == 'w' ? RenderColor{217, 199, 159, 255}
                                : c == 'b' ? RenderColor{183, 131, 80, 255}
                                : c == 'h' ? RenderColor{136, 96, 68, 255}
                                           : RenderColor{73, 53, 47, 255};
                            renderer.fillRectangle(
                                point->x + (x - 8) * step,
                                point->y + (y - 13) * step,
                                step,
                                step,
                                visibleColor(
                                    color,
                                    point->visibility * armyVisibility
                                )
                            );
                        }
                    }
                }
            }

            if (artwork)
            {
                struct ArtItem
                {
                    const SceneSprite* sprite;
                    ProjectedWorldObject point;
                    double scale;
                    int pose;
                    float visibility;
                    bool selected = false;
                };
                std::vector<ArtItem> items;
                // Settlement sprawl is removed. Only universal native map
                // symbols remain.
                for (const auto& army : world.armies())
                {
                    if (army.garrisoned() || army.soldierCount() == 0 ||
                        armyVisibility <= .001F)
                    {
                        continue;
                    }
                    // One strategic representative, regardless of roster size.
                    // Actual strength is the native-screen count below the
                    // sprite.
                    const auto* sprite = worldArmySprite(*artwork, world, army);
                    const auto point =
                        project(army.visualX() + .5, army.visualY() + .5);
                    if (sprite && sprite->texture && point &&
                        !outside(*point, renderer, 256))
                    {
                        items.push_back(
                            {sprite,
                             *point,
                             worldArmySpriteScale(effectiveTilePixels, sprite),
                             army.moving()
                                 ? int(std::floor(army.marchDistance() * 4)) % 4
                                 : 0,
                             armyVisibility,
                             army.id() == selectedArmy}
                        );
                    }
                }
                std::stable_sort(
                    items.begin(),
                    items.end(),
                    [](const auto& a, const auto& b)
                    { return a.point.y < b.point.y; }
                );
                // Atlas frames retain nearest filtering and one common 32-texel
                // strategic lattice. The common source-camera residual happens
                // once.
                for (const auto& item : items)
                {
                    auto source = artwork->frame(*item.sprite, false);
                    source.x +=
                        float(item.pose % std::max(1, item.sprite->frames)) *
                        source.width;
                    const float w = float(
                        item.sprite->width * item.scale * effectiveTilePixels
                    );
                    const float h = float(
                        item.sprite->height * item.scale * effectiveTilePixels
                    );
                    if (item.selected && item.sprite->selectionSilhouette)
                    {
                        // Eight offset alpha silhouettes, hidden by the
                        // sprite's interior, produce a contour rather than a
                        // selection box.
                        const float step =
                            float(worldObjectPixelPitch(effectiveTilePixels));
                        for (int dy = -1; dy <= 1; ++dy)
                        {
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                if (dx || dy)
                                {
                                    renderer.drawTexture(
                                        *item.sprite->selectionSilhouette,
                                        source.x,
                                        source.y,
                                        source.width,
                                        source.height,
                                        item.point.x -
                                            w * float(item.sprite->pivotX) +
                                            dx * step,
                                        item.point.y -
                                            h * float(item.sprite->pivotY) +
                                            dy * step,
                                        w,
                                        h,
                                        visibleColor(
                                            {255, 255, 255, 255},
                                            item.visibility *
                                                item.point.visibility
                                        )
                                            .alpha
                                    );
                                }
                            }
                        }
                    }
                    renderer.drawTexture(
                        *item.sprite->texture,
                        source.x,
                        source.y,
                        source.width,
                        source.height,
                        item.point.x - w * float(item.sprite->pivotX),
                        item.point.y - h * float(item.sprite->pivotY),
                        w,
                        h,
                        visibleColor(
                            {255, 255, 255, 255},
                            item.visibility * item.point.visibility
                        )
                            .alpha
                    );
                }
            }
        }

        // Native-resolution cartography is not squeezed through the world art
        // layer. Every symbol has immutable local geometry; only its whole
        // plate origin moves, using the snapped source camera plus the
        // terrain's SAME final integer-screen residual. Universal symbols never
        // change with population.
        const auto annotationProject =
            [&](double x, double y) -> std::optional<ProjectedWorldObject>
        {
            auto p = project(x, y);
            if (!p)
            {
                return p;
            }
            if (stabilizePixelPhase)
            {
                p->x = std::round(p->x);
                p->y = std::round(p->y);
            }
            p->x += float(rigidResidual.x);
            p->y += float(rigidResidual.y);
            return p;
        };
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
                if (worldObjectVisibility <= .001F)
                {
                    continue;
                }
                const auto position = settlement.position();
                auto point = annotationProject(
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
            struct ArmyPlate
            {
                ProjectedWorldObject point;
                UiRectangle body;
            };
            std::vector<ArmyPlate> armyPlates;
            armyPlates.reserve(world.armies().size());
            if (artwork)
            {
                for (const auto& army : world.armies())
                {
                    if (army.garrisoned() || !army.soldierCount() ||
                        armyVisibility <= .001F)
                    {
                        continue;
                    }
                    const auto point = annotationProject(
                        army.visualX() + .5,
                        army.visualY() + .5
                    );
                    const auto* sprite = worldArmySprite(*artwork, world, army);
                    if (point && sprite && sprite->texture &&
                        !outside(*point, renderer, 256))
                    {
                        armyPlates.push_back(
                            {*point,
                             worldArmySpriteBounds(
                                 point->x,
                                 point->y,
                                 effectiveTilePixels,
                                 sprite
                             )}
                        );
                    }
                }
            }
            for (const auto& item : settlements)
            {
                float clearance = 0.F;
                bool showSymbol = true;
                for (const auto& army : armyPlates)
                {
                    // Preserve geographic anchors; move only the native text
                    // away from the soldier rather than drawing over his head.
                    if (std::abs(army.point.x - item.point.x) <
                            std::max(16.F, army.body.width * .5F) &&
                        std::abs(army.point.y - item.point.y) < 24.F)
                    {
                        clearance = std::max(
                            clearance,
                            item.point.y - army.body.y + 4.F
                        );
                        showSymbol = false;
                    }
                }
                settlementMarkerRenderer_.drawAt(
                    renderer,
                    world,
                    *item.settlement,
                    item.point.x,
                    item.point.y,
                    item.point.visibility,
                    clearance,
                    showSymbol
                );
            }

            for (const Army& army : world.armies())
            {
                if (army.garrisoned() || army.soldierCount() == 0 ||
                    armyVisibility <= .001F)
                {
                    continue;
                }
                const auto point = annotationProject(
                    army.visualX() + 0.5,
                    army.visualY() + 0.5
                );
                if (!point || outside(*point, renderer, 64.0F))
                {
                    continue;
                }
                const auto* sprite =
                    artwork ? worldArmySprite(*artwork, world, army) : nullptr;
                const auto label = worldArmyCountBounds(
                    point->x,
                    point->y,
                    army.soldierCount(),
                    effectiveTilePixels,
                    sprite
                );
                const float countVisibility =
                    point->visibility * armyVisibility;
                renderer.fillRectangle(
                    label.x,
                    label.y,
                    label.width,
                    label.height,
                    visibleColor({8, 15, 27, 235}, countVisibility)
                );
                BitmapFontRenderer{}.drawText(
                    renderer,
                    std::to_string(army.soldierCount()),
                    label.x + 4,
                    label.y + 3,
                    2.F,
                    visibleColor({239, 226, 207, 255}, countVisibility)
                );
            }
        }

        if (placementMarker)
        {
            const auto point = annotationProject(
                double(placementMarker->position.x) + 0.5,
                double(placementMarker->position.y) + 0.5
            );
            if (point && !outside(*point, renderer, 64.0F))
            {
                RenderColor color =
                    visibleColor(placementMarker->color, point->visibility);
                RenderColor shadow =
                    visibleColor({8, 15, 27, 220}, point->visibility);
                const float radius =
                    std::clamp(float(effectiveTilePixels * 0.24), 5.0F, 10.0F);
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
