#pragma once

#include "core/StrongId.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldPresentation.h"
#include "ui/BitmapFontRenderer.h"
#include "world/Realm.h"
#include "world/World.h"
#include "world/WorldTilePosition.h"
#include "world/territory/TerritoryMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    // Projection-independent realm presentation for the world screen.  The
    // simulation remains tile based; this renderer owns only the cartographic
    // fill, realm-edge presentation and distant realm labels.
    class WorldTerritoryPresentationRenderer
    {
        enum class TileEdge
        {
            Left,
            Right,
            Top,
            Bottom
        };

        struct BoundaryEdge
        {
            WorldTilePosition position;
            TileEdge edge = TileEdge::Left;
            RealmId realmId;
        };

        struct RealmLabelPlacement
        {
            RealmId realmId;
            std::uint64_t tileCount = 0;
            double sumLongitudeSin = 0.0;
            double sumLongitudeCos = 0.0;
            double sumY = 0.0;
            WorldTilePosition anchor;
            double anchorDistanceSquared =
                std::numeric_limits<double>::max();
        };

        struct NeighborEdge
        {
            WorldTilePosition offset;
            TileEdge edge;
        };

        static constexpr std::array<NeighborEdge, 4> NeighborEdges{
            NeighborEdge{{-1, 0}, TileEdge::Left},
            NeighborEdge{{1, 0}, TileEdge::Right},
            NeighborEdge{{0, -1}, TileEdge::Top},
            NeighborEdge{{0, 1}, TileEdge::Bottom}
        };
        static constexpr double Pi = 3.14159265358979323846;
        static constexpr int SphereColumns = 192;
        static constexpr int SphereRows = 96;

        const TerritoryMap* source_ = nullptr;
        std::uint64_t territoryRevision_ =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t realmSignature_ = 0;
        std::unique_ptr<Texture> politicalOverlay_;
        std::vector<RenderColor> overlayPixels_;
        std::vector<BoundaryEdge> boundaryEdges_;
        std::vector<RealmLabelPlacement> labelPlacements_;
        std::vector<WorldSurface::Point3> spherePoints_;
        BitmapFontRenderer fontRenderer_;

        [[nodiscard]]
        static std::uint64_t mixSignature(
            std::uint64_t signature,
            std::uint64_t value
        ) noexcept
        {
            signature ^= value;
            signature *= 1099511628211ULL;
            return signature;
        }

        [[nodiscard]]
        static std::uint64_t realmSignature(
            std::span<const Realm> realms
        ) noexcept
        {
            std::uint64_t signature = 1469598103934665603ULL;
            for (const Realm& realm : realms)
            {
                const MapColor color = realm.mapColor();
                signature = mixSignature(signature, realm.id().value());
                signature = mixSignature(signature, color.red);
                signature = mixSignature(signature, color.green);
                signature = mixSignature(signature, color.blue);
                for (const char character : realm.name())
                {
                    signature = mixSignature(
                        signature,
                        static_cast<unsigned char>(character)
                    );
                }
            }
            return signature;
        }

        [[nodiscard]]
        static RenderColor visibleColor(
            RenderColor color,
            float visibility
        ) noexcept
        {
            const float weight = std::clamp(visibility, 0.0F, 1.0F);
            color.alpha = static_cast<std::uint8_t>(std::clamp(
                std::lround(double(color.alpha) * weight),
                0L,
                255L
            ));
            return color;
        }

        [[nodiscard]]
        static RenderColor realmColor(
            const Realm& realm,
            float visibility = 1.0F
        ) noexcept
        {
            const MapColor color = realm.mapColor();
            return visibleColor(
                {color.red, color.green, color.blue, 255},
                visibility
            );
        }

        void ensureSpherePoints()
        {
            const std::size_t required =
                std::size_t(SphereColumns + 1) * (SphereRows + 1);
            if (spherePoints_.size() == required)
            {
                return;
            }

            spherePoints_.resize(required);
            for (int row = 0; row <= SphereRows; ++row)
            {
                for (int column = 0; column <= SphereColumns; ++column)
                {
                    const double u = double(column) / SphereColumns;
                    const double v = double(row) / SphereRows;
                    spherePoints_[std::size_t(row) * (SphereColumns + 1) +
                                  column] = WorldSurface::sphere(u, v);
                }
            }
        }

        void ensureCache(Renderer& renderer, const World& world)
        {
            const TerritoryMap& territory = world.territory();
            const auto realms = world.realms();
            const std::uint64_t currentRealmSignature = realmSignature(realms);

            if (source_ == &territory &&
                territoryRevision_ == territory.revision() &&
                realmSignature_ == currentRealmSignature)
            {
                return;
            }

            source_ = &territory;
            territoryRevision_ = territory.revision();
            realmSignature_ = currentRealmSignature;
            boundaryEdges_.clear();
            labelPlacements_.clear();

            const int width = territory.width();
            const int height = territory.height();
            if (width <= 0 || height <= 0)
            {
                politicalOverlay_.reset();
                overlayPixels_.clear();
                return;
            }

            overlayPixels_.assign(
                std::size_t(width) * height,
                RenderColor{0, 0, 0, 0}
            );

            std::unordered_map<RealmId, std::size_t, StrongIdHash>
                placementByRealm;
            labelPlacements_.resize(realms.size());
            for (std::size_t index = 0; index < realms.size(); ++index)
            {
                labelPlacements_[index].realmId = realms[index].id();
                placementByRealm.emplace(realms[index].id(), index);
            }

            for (const WorldTilePosition position :
                 territory.controlledPositions())
            {
                const RealmId controller = territory.controllerAt(position);
                const Realm* realm = world.realm(controller);
                if (!realm)
                {
                    continue;
                }

                const std::size_t tileIndex =
                    std::size_t(position.y) * width + position.x;
                overlayPixels_[tileIndex] = realmColor(*realm);

                for (const NeighborEdge& neighborEdge : NeighborEdges)
                {
                    int neighborX = position.x + neighborEdge.offset.x;
                    const int neighborY = position.y + neighborEdge.offset.y;
                    RealmId neighborController;
                    if (neighborY >= 0 && neighborY < height)
                    {
                        neighborX = (neighborX % width + width) % width;
                        neighborController = territory.controllerAt(
                            {neighborX, neighborY}
                        );
                    }
                    if (neighborController != controller)
                    {
                        boundaryEdges_.push_back(
                            {position, neighborEdge.edge, controller}
                        );
                    }
                }

                const auto placementIterator =
                    placementByRealm.find(controller);
                if (placementIterator == placementByRealm.end())
                {
                    continue;
                }

                RealmLabelPlacement& placement =
                    labelPlacements_[placementIterator->second];
                const double longitude =
                    (double(position.x) + 0.5) / width * 2.0 * Pi;
                ++placement.tileCount;
                placement.sumLongitudeSin += std::sin(longitude);
                placement.sumLongitudeCos += std::cos(longitude);
                placement.sumY += double(position.y) + 0.5;
            }

            for (const WorldTilePosition position :
                 territory.controlledPositions())
            {
                const auto placementIterator = placementByRealm.find(
                    territory.controllerAt(position)
                );
                if (placementIterator == placementByRealm.end())
                {
                    continue;
                }

                RealmLabelPlacement& placement =
                    labelPlacements_[placementIterator->second];
                if (placement.tileCount == 0)
                {
                    continue;
                }

                double longitude = std::atan2(
                    placement.sumLongitudeSin,
                    placement.sumLongitudeCos
                );
                if (longitude < 0.0)
                {
                    longitude += 2.0 * Pi;
                }
                const double centerX = longitude / (2.0 * Pi) * width;
                const double centerY =
                    placement.sumY / double(placement.tileCount);
                double deltaX =
                    std::abs((double(position.x) + 0.5) - centerX);
                deltaX = std::min(deltaX, double(width) - deltaX);
                const double deltaY =
                    (double(position.y) + 0.5) - centerY;
                const double distanceSquared =
                    deltaX * deltaX + deltaY * deltaY;
                if (distanceSquared < placement.anchorDistanceSquared)
                {
                    placement.anchorDistanceSquared = distanceSquared;
                    placement.anchor = position;
                }
            }

            bool updated = false;
            if (politicalOverlay_ && politicalOverlay_->width() == width &&
                politicalOverlay_->height() == height)
            {
                updated = renderer.updateTexturePixels(
                    *politicalOverlay_,
                    overlayPixels_
                );
            }
            if (!updated)
            {
                politicalOverlay_ = renderer.createTextureFromPixels(
                    width,
                    height,
                    overlayPixels_
                );
            }
            if (politicalOverlay_)
            {
                renderer.setTextureFiltering(*politicalOverlay_, false);
            }
        }

        [[nodiscard]]
        static std::array<WorldSurface::UV, 2> boundaryUv(
            const BoundaryEdge& boundary,
            int width,
            int height
        ) noexcept
        {
            const double x = double(boundary.position.x);
            const double y = double(boundary.position.y);
            switch (boundary.edge)
            {
            case TileEdge::Left:
                return {{{x / width, y / height},
                         {x / width, (y + 1.0) / height}}};
            case TileEdge::Right:
                return {{{(x + 1.0) / width, y / height},
                         {(x + 1.0) / width, (y + 1.0) / height}}};
            case TileEdge::Top:
                return {{{x / width, y / height},
                         {(x + 1.0) / width, y / height}}};
            case TileEdge::Bottom:
                return {{{x / width, (y + 1.0) / height},
                         {(x + 1.0) / width, (y + 1.0) / height}}};
            }
            return {};
        }

        void drawRealmLabel(
            Renderer& renderer,
            const Realm& realm,
            const RealmLabelPlacement& placement,
            float centerX,
            float centerY,
            double tilePixels,
            float visibility,
            const WorldPresentationPolicy& policy
        ) const
        {
            if (visibility <= 0.0F || placement.tileCount == 0 ||
                realm.name().empty())
            {
                return;
            }

            const float naturalWidth =
                fontRenderer_.measureWidth(realm.name(), 1.0F);
            const double representativeWidthTiles =
                std::max(1.0, std::sqrt(double(placement.tileCount)) * 1.8);
            const float minimumLabel =
                std::max(0.5F, policy.minimumRealmLabelPixelSize);
            const float maximumLabel =
                std::max(minimumLabel, policy.maximumRealmLabelPixelSize);
            const float pixelSize = std::clamp(
                float(representativeWidthTiles * tilePixels) *
                    policy.realmLabelTerritoryWidthFraction /
                    std::max(naturalWidth, 1.0F),
                minimumLabel,
                maximumLabel
            );
            const float labelWidth =
                fontRenderer_.measureWidth(realm.name(), pixelSize);

            const MapColor mapColor = realm.mapColor();
            const int luminance = 299 * int(mapColor.red) +
                                  587 * int(mapColor.green) +
                                  114 * int(mapColor.blue);
            const bool useDarkText = luminance > 145000;
            const RenderColor shadow = visibleColor(
                useDarkText ? RenderColor{255, 255, 255, 210}
                            : RenderColor{8, 15, 27, 225},
                visibility
            );
            const RenderColor text = visibleColor(
                useDarkText ? RenderColor{24, 27, 32, 255}
                            : RenderColor{244, 243, 232, 255},
                visibility
            );
            const float labelX = centerX - labelWidth * 0.5F;
            const float labelY = centerY - 3.5F * pixelSize;
            fontRenderer_.drawText(
                renderer,
                realm.name(),
                labelX + 1.0F,
                labelY + 1.0F,
                pixelSize,
                shadow
            );
            fontRenderer_.drawText(
                renderer,
                realm.name(),
                labelX,
                labelY,
                pixelSize,
                text
            );
        }

        void drawBorderLine(
            Renderer& renderer,
            float x1,
            float y1,
            float x2,
            float y2,
            RenderColor color,
            float visibility
        ) const
        {
            if (visibility <= 0.0F)
            {
                return;
            }
            const RenderColor backdrop = visibleColor(
                {8, 15, 27, 175},
                visibility
            );
            const double dx = double(x2) - x1;
            const double dy = double(y2) - y1;
            const double length = std::hypot(dx, dy);
            if (length > 0.001)
            {
                const float nx = float(-dy / length);
                const float ny = float(dx / length);
                renderer.drawLine(
                    x1 + nx,
                    y1 + ny,
                    x2 + nx,
                    y2 + ny,
                    backdrop
                );
                renderer.drawLine(
                    x1 - nx,
                    y1 - ny,
                    x2 - nx,
                    y2 - ny,
                    backdrop
                );
            }
            renderer.drawLine(x1, y1, x2, y2, visibleColor(color, visibility));
        }

    public:
        void reset()
        {
            source_ = nullptr;
            territoryRevision_ = std::numeric_limits<std::uint64_t>::max();
            realmSignature_ = 0;
            politicalOverlay_.reset();
            overlayPixels_.clear();
            boundaryEdges_.clear();
            labelPlacements_.clear();
        }

        void renderFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        )
        {
            ensureCache(renderer, world);
            const TerritoryMap& territory = world.territory();
            if (territory.controlledTileCount() == 0)
            {
                return;
            }

            const double tilePixels = metrics.scaledTilePixels(camera.zoom());
            if (!std::isfinite(tilePixels) || tilePixels <= 0.0)
            {
                return;
            }
            const double viewportWidth = renderer.outputWidth();
            const double viewportHeight = renderer.outputHeight();

            if (politicalOverlay_ && presentation.realmFillWeight > 0.001F)
            {
                renderer.drawTexture(
                    *politicalOverlay_,
                    0.0F,
                    0.0F,
                    float(territory.width()),
                    float(territory.height()),
                    float(viewportWidth * 0.5 - camera.tileX() * tilePixels),
                    float(viewportHeight * 0.5 - camera.tileY() * tilePixels),
                    float(territory.width() * tilePixels),
                    float(territory.height() * tilePixels),
                    static_cast<std::uint8_t>(std::clamp(
                        std::lround(255.0 * presentation.realmFillWeight),
                        0L,
                        255L
                    ))
                );
            }

            if (presentation.realmBorderWeight > 0.001F)
            {
                for (const BoundaryEdge& boundary : boundaryEdges_)
                {
                    const Realm* realm = world.realm(boundary.realmId);
                    if (!realm)
                    {
                        continue;
                    }
                    const auto uv = boundaryUv(
                        boundary,
                        territory.width(),
                        territory.height()
                    );
                    const float x1 = float(
                        viewportWidth * 0.5 +
                        (uv[0].u * territory.width() - camera.tileX()) *
                            tilePixels
                    );
                    const float y1 = float(
                        viewportHeight * 0.5 +
                        (uv[0].v * territory.height() - camera.tileY()) *
                            tilePixels
                    );
                    const float x2 = float(
                        viewportWidth * 0.5 +
                        (uv[1].u * territory.width() - camera.tileX()) *
                            tilePixels
                    );
                    const float y2 = float(
                        viewportHeight * 0.5 +
                        (uv[1].v * territory.height() - camera.tileY()) *
                            tilePixels
                    );
                    if ((x1 < 0.0F && x2 < 0.0F) ||
                        (y1 < 0.0F && y2 < 0.0F) ||
                        (x1 > viewportWidth && x2 > viewportWidth) ||
                        (y1 > viewportHeight && y2 > viewportHeight))
                    {
                        continue;
                    }
                    drawBorderLine(
                        renderer,
                        x1,
                        y1,
                        x2,
                        y2,
                        realmColor(*realm),
                        presentation.realmBorderWeight
                    );
                }
            }

            if (presentation.realmLabelWeight <= 0.001F)
            {
                return;
            }
            for (const RealmLabelPlacement& placement : labelPlacements_)
            {
                const Realm* realm = world.realm(placement.realmId);
                if (!realm || placement.tileCount == 0)
                {
                    continue;
                }
                const float centerX = float(
                    viewportWidth * 0.5 +
                    (double(placement.anchor.x) + 0.5 - camera.tileX()) *
                        tilePixels
                );
                const float centerY = float(
                    viewportHeight * 0.5 +
                    (double(placement.anchor.y) + 0.5 - camera.tileY()) *
                        tilePixels
                );
                drawRealmLabel(
                    renderer,
                    *realm,
                    placement,
                    centerX,
                    centerY,
                    tilePixels,
                    presentation.realmLabelWeight,
                    policy
                );
            }
        }

        void renderGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        )
        {
            ensureCache(renderer, world);
            const TerritoryMap& territory = world.territory();
            if (territory.controlledTileCount() == 0 ||
                renderer.outputWidth() <= 0 || renderer.outputHeight() <= 0)
            {
                return;
            }

            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            const double tilePixels =
                view.radius * 2.0 * Pi / world.grid().width();

            if (politicalOverlay_ && presentation.realmFillWeight > 0.001F)
            {
                ensureSpherePoints();
                std::vector<MeshVertex> vertices(spherePoints_.size());
                std::vector<double> depths(spherePoints_.size());
                const RenderColor vertexColor{
                    255,
                    255,
                    255,
                    static_cast<std::uint8_t>(std::clamp(
                        std::lround(255.0 * presentation.realmFillWeight),
                        0L,
                        255L
                    ))
                };

                for (int row = 0; row <= SphereRows; ++row)
                {
                    for (int column = 0; column <= SphereColumns; ++column)
                    {
                        const std::size_t index =
                            std::size_t(row) * (SphereColumns + 1) + column;
                        const auto point = view.orient(spherePoints_[index]);
                        depths[index] = point.z;
                        vertices[index] = {
                            float(view.cx + point.x * view.radius),
                            float(view.cy - point.y * view.radius),
                            float(column) / SphereColumns,
                            float(row) / SphereRows,
                            vertexColor
                        };
                    }
                }

                std::vector<int> indices;
                indices.reserve(std::size_t(SphereColumns) * SphereRows * 6);
                for (int row = 0; row < SphereRows; ++row)
                {
                    for (int column = 0; column < SphereColumns; ++column)
                    {
                        const int a = row * (SphereColumns + 1) + column;
                        const int b = a + 1;
                        const int d = (row + 1) * (SphereColumns + 1) + column;
                        const int c = d + 1;
                        if (depths[a] > 0.0 && depths[b] > 0.0 &&
                            depths[c] > 0.0)
                        {
                            indices.push_back(a);
                            indices.push_back(b);
                            indices.push_back(c);
                        }
                        if (depths[a] > 0.0 && depths[c] > 0.0 &&
                            depths[d] > 0.0)
                        {
                            indices.push_back(a);
                            indices.push_back(c);
                            indices.push_back(d);
                        }
                    }
                }
                if (!indices.empty())
                {
                    renderer.drawMesh(*politicalOverlay_, vertices, indices);
                }
            }

            if (presentation.realmBorderWeight > 0.001F)
            {
                for (const BoundaryEdge& boundary : boundaryEdges_)
                {
                    const Realm* realm = world.realm(boundary.realmId);
                    if (!realm)
                    {
                        continue;
                    }
                    const auto uv = boundaryUv(
                        boundary,
                        territory.width(),
                        territory.height()
                    );
                    const auto p1 = view.project(uv[0].u, uv[0].v);
                    const auto p2 = view.project(uv[1].u, uv[1].v);
                    if (p1.z <= 0.01 || p2.z <= 0.01)
                    {
                        continue;
                    }
                    drawBorderLine(
                        renderer,
                        float(p1.x),
                        float(p1.y),
                        float(p2.x),
                        float(p2.y),
                        realmColor(*realm),
                        presentation.realmBorderWeight
                    );
                }
            }

            if (presentation.realmLabelWeight <= 0.001F)
            {
                return;
            }
            for (const RealmLabelPlacement& placement : labelPlacements_)
            {
                const Realm* realm = world.realm(placement.realmId);
                if (!realm || placement.tileCount == 0)
                {
                    continue;
                }
                const auto projected = view.project(
                    (double(placement.anchor.x) + 0.5) / territory.width(),
                    (double(placement.anchor.y) + 0.5) / territory.height()
                );
                if (projected.z <= 0.15)
                {
                    continue;
                }
                drawRealmLabel(
                    renderer,
                    *realm,
                    placement,
                    float(projected.x),
                    float(projected.y),
                    tilePixels,
                    presentation.realmLabelWeight *
                        std::clamp(float(projected.z * 3.0), 0.0F, 1.0F),
                    policy
                );
            }
        }
    };
} // namespace Paladin
