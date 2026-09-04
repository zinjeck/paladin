#include "rendering/TerritoryRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TerritoryPresentationPolicy.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/Polity.h"
#include "world/World.h"
#include "world/territory/TerritoryMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    namespace
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
            PolityId polityId;
        };

        struct PolityLabelPlacement
        {
            PolityId polityId;
            std::uint64_t tileCount = 0;
            std::int64_t sumX = 0;
            std::int64_t sumY = 0;
            std::int32_t minimumX =
                std::numeric_limits<std::int32_t>::max();
            std::int32_t maximumX =
                std::numeric_limits<std::int32_t>::min();
            WorldTilePosition anchor;
            double anchorDistanceSquared =
                std::numeric_limits<double>::max();
        };

        struct NeighborEdge
        {
            WorldTilePosition offset;
            TileEdge edge;
        };

        constexpr std::array<NeighborEdge, 4> neighborEdges{
            NeighborEdge{{-1, 0}, TileEdge::Left},
            NeighborEdge{{1, 0}, TileEdge::Right},
            NeighborEdge{{0, -1}, TileEdge::Top},
            NeighborEdge{{0, 1}, TileEdge::Bottom}
        };

        RenderColor polityColor(
            const Polity& polity,
            std::uint8_t alpha
        ) noexcept
        {
            const MapColor color = polity.mapColor();
            return {
                color.red,
                color.green,
                color.blue,
                alpha
            };
        }

        RenderRectangle tileEdgeRectangle(
            float tileX,
            float tileY,
            float tilePixels,
            TileEdge edge,
            float width
        ) noexcept
        {
            const float safeWidth = std::clamp(
                width,
                0.5F,
                std::max(0.5F, tilePixels * 0.5F)
            );

            switch (edge)
            {
                case TileEdge::Left:
                    return {
                        tileX,
                        tileY,
                        safeWidth,
                        tilePixels
                    };

                case TileEdge::Right:
                    return {
                        tileX + tilePixels - safeWidth,
                        tileY,
                        safeWidth,
                        tilePixels
                    };

                case TileEdge::Top:
                    return {
                        tileX,
                        tileY,
                        tilePixels,
                        safeWidth
                    };

                case TileEdge::Bottom:
                    return {
                        tileX,
                        tileY + tilePixels - safeWidth,
                        tilePixels,
                        safeWidth
                    };
            }

            return {};
        }

        std::uint64_t mixSignature(
            std::uint64_t signature,
            std::uint64_t value
        ) noexcept
        {
            signature ^= value;
            signature *= 1099511628211ULL;
            return signature;
        }

        std::uint64_t politySignature(
            std::span<const Polity> polities
        ) noexcept
        {
            std::uint64_t signature = 1469598103934665603ULL;

            for (const Polity& polity : polities)
            {
                const MapColor color = polity.mapColor();
                signature = mixSignature(
                    signature,
                    polity.id().value()
                );
                signature = mixSignature(signature, color.red);
                signature = mixSignature(signature, color.green);
                signature = mixSignature(signature, color.blue);

                for (const char character : polity.name())
                {
                    signature = mixSignature(
                        signature,
                        static_cast<unsigned char>(character)
                    );
                }
            }

            return signature;
        }
    }


    struct TerritoryRendererCache
    {
        const TerritoryMap* source = nullptr;
        std::uint64_t territoryRevision =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t politySignature = 0;
        std::uint8_t fillAlpha = 0;
        std::unique_ptr<Texture> politicalOverlay;
        std::vector<RenderColor> overlayPixels;
        std::vector<std::size_t> paintedPixelIndices;
        std::vector<BoundaryEdge> boundaryEdges;
        std::vector<PolityLabelPlacement> labelPlacements;
    };


    TerritoryRenderer::TerritoryRenderer()
        : cache_(std::make_unique<TerritoryRendererCache>())
    {
    }


    TerritoryRenderer::~TerritoryRenderer() = default;


    void TerritoryRenderer::render(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        bool politicalView,
        const TerritoryPresentationPolicy& policy
    ) const
    {
        const TerritoryMap& territory = world.territory();

        if (territory.controlledTileCount() == 0)
        {
            return;
        }

        const double tilePixelsDouble =
            metrics.scaledTilePixels(camera.zoom());

        if (tilePixelsDouble <= 0.0)
        {
            return;
        }

        const std::span<const Polity> polities = world.polities();
        const std::uint64_t currentPolitySignature =
            politySignature(polities);

        if (
            cache_->source != &territory ||
            cache_->territoryRevision != territory.revision() ||
            cache_->politySignature != currentPolitySignature ||
            cache_->fillAlpha != policy.politicalFillAlpha
        )
        {
            const bool sourceChanged =
                cache_->source != &territory;

            cache_->source = &territory;
            cache_->territoryRevision = territory.revision();
            cache_->politySignature = currentPolitySignature;
            cache_->fillAlpha = policy.politicalFillAlpha;
            cache_->boundaryEdges.clear();
            cache_->labelPlacements.clear();

            const std::size_t overlayPixelCount =
                static_cast<std::size_t>(territory.width())
                * static_cast<std::size_t>(territory.height());

            if (
                sourceChanged ||
                cache_->overlayPixels.size() != overlayPixelCount
            )
            {
                cache_->overlayPixels.assign(
                    overlayPixelCount,
                    RenderColor{0, 0, 0, 0}
                );
                cache_->paintedPixelIndices.clear();
            }
            else
            {
                for (const std::size_t tileIndex
                    : cache_->paintedPixelIndices)
                {
                    cache_->overlayPixels[tileIndex] =
                        RenderColor{0, 0, 0, 0};
                }

                cache_->paintedPixelIndices.clear();
            }

            std::unordered_map<
                PolityId,
                std::size_t,
                StrongIdHash
            > placementByPolity;

            cache_->labelPlacements.resize(polities.size());

            for (
                std::size_t index = 0;
                index < polities.size();
                ++index
            )
            {
                cache_->labelPlacements[index].polityId =
                    polities[index].id();

                placementByPolity.emplace(
                    polities[index].id(),
                    index
                );
            }

            for (const WorldTilePosition position
                : territory.controlledPositions())
            {
                const std::int32_t x = position.x;
                const std::int32_t y = position.y;
                const PolityId controller =
                    territory.controllerAt(position);
                const Polity* polity = world.polity(controller);

                if (!polity)
                {
                    continue;
                }

                const std::size_t tileIndex =
                    static_cast<std::size_t>(y)
                        * static_cast<std::size_t>(
                            territory.width()
                        )
                    + static_cast<std::size_t>(x);

                cache_->overlayPixels[tileIndex] = polityColor(
                    *polity,
                    policy.politicalFillAlpha
                );
                cache_->paintedPixelIndices.push_back(tileIndex);

                for (const NeighborEdge& neighborEdge
                    : neighborEdges)
                {
                    const WorldTilePosition neighbor{
                        x + neighborEdge.offset.x,
                        y + neighborEdge.offset.y
                    };

                    if (
                        territory.controllerAt(neighbor)
                        != controller
                    )
                    {
                        cache_->boundaryEdges.push_back({
                            position,
                            neighborEdge.edge,
                            controller
                        });
                    }
                }

                const auto placementIterator =
                    placementByPolity.find(controller);

                if (
                    placementIterator
                    == placementByPolity.end()
                )
                {
                    continue;
                }

                PolityLabelPlacement& placement =
                    cache_->labelPlacements[
                        placementIterator->second
                    ];

                ++placement.tileCount;
                placement.sumX += x;
                placement.sumY += y;
                placement.minimumX =
                    std::min(placement.minimumX, x);
                placement.maximumX =
                    std::max(placement.maximumX, x);
            }

            for (const WorldTilePosition position
                : territory.controlledPositions())
            {
                const std::int32_t x = position.x;
                const std::int32_t y = position.y;
                const auto placementIterator =
                    placementByPolity.find(
                        territory.controllerAt(position)
                    );

                if (
                    placementIterator
                    == placementByPolity.end()
                )
                {
                    continue;
                }

                PolityLabelPlacement& placement =
                    cache_->labelPlacements[
                        placementIterator->second
                    ];

                const double centerX =
                    static_cast<double>(placement.sumX)
                    / static_cast<double>(placement.tileCount);

                const double centerY =
                    static_cast<double>(placement.sumY)
                    / static_cast<double>(placement.tileCount);

                const double deltaX =
                    static_cast<double>(x) - centerX;

                const double deltaY =
                    static_cast<double>(y) - centerY;

                const double distanceSquared =
                    deltaX * deltaX + deltaY * deltaY;

                if (
                    distanceSquared
                    < placement.anchorDistanceSquared
                )
                {
                    placement.anchorDistanceSquared =
                        distanceSquared;
                    placement.anchor = {x, y};
                }
            }

            bool overlayUpdated = false;

            if (
                cache_->politicalOverlay &&
                cache_->politicalOverlay->width()
                    == territory.width() &&
                cache_->politicalOverlay->height()
                    == territory.height()
            )
            {
                overlayUpdated = renderer.updateTexturePixels(
                    *cache_->politicalOverlay,
                    cache_->overlayPixels
                );
            }

            if (!overlayUpdated)
            {
                cache_->politicalOverlay =
                    renderer.createTextureFromPixels(
                        territory.width(),
                        territory.height(),
                        cache_->overlayPixels
                    );
            }
        }

        const double viewportWidth =
            static_cast<double>(renderer.outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer.outputHeight());

        if (politicalView && cache_->politicalOverlay)
        {
            renderer.drawTexture(
                *cache_->politicalOverlay,
                0.0F,
                0.0F,
                static_cast<float>(territory.width()),
                static_cast<float>(territory.height()),
                static_cast<float>(
                    viewportWidth * 0.5
                    - camera.tileX() * tilePixelsDouble
                ),
                static_cast<float>(
                    viewportHeight * 0.5
                    - camera.tileY() * tilePixelsDouble
                ),
                static_cast<float>(
                    static_cast<double>(territory.width())
                    * tilePixelsDouble
                ),
                static_cast<float>(
                    static_cast<double>(territory.height())
                    * tilePixelsDouble
                )
            );
        }

        const float tilePixels =
            static_cast<float>(tilePixelsDouble);

        const float borderWidth = politicalView
            ? policy.politicalViewBorderWidth
            : policy.closeViewBorderWidth;

        const RenderColor backdrop{6, 9, 14, 205};

        std::vector<RenderRectangle> backdropRectangles;
        backdropRectangles.reserve(cache_->boundaryEdges.size());

        std::unordered_map<
            PolityId,
            std::vector<RenderRectangle>,
            StrongIdHash
        > borderRectanglesByPolity;

        for (const BoundaryEdge& boundary : cache_->boundaryEdges)
        {
            const float screenX = static_cast<float>(
                viewportWidth * 0.5
                + (
                    static_cast<double>(boundary.position.x)
                    - camera.tileX()
                ) * tilePixelsDouble
            );

            const float screenY = static_cast<float>(
                viewportHeight * 0.5
                + (
                    static_cast<double>(boundary.position.y)
                    - camera.tileY()
                ) * tilePixelsDouble
            );

            if (
                screenX + tilePixels < 0.0F ||
                screenY + tilePixels < 0.0F ||
                screenX > viewportWidth ||
                screenY > viewportHeight
            )
            {
                continue;
            }

            backdropRectangles.push_back(
                tileEdgeRectangle(
                    screenX,
                    screenY,
                    tilePixels,
                    boundary.edge,
                    borderWidth
                        + policy.borderBackdropExtraWidth
                )
            );

            borderRectanglesByPolity[boundary.polityId]
                .push_back(
                    tileEdgeRectangle(
                        screenX,
                        screenY,
                        tilePixels,
                        boundary.edge,
                        borderWidth
                    )
                );
        }

        renderer.fillRectangles(
            backdropRectangles,
            backdrop
        );

        for (const auto& [polityId, rectangles]
            : borderRectanglesByPolity)
        {
            const Polity* polity = world.polity(polityId);

            if (!polity)
            {
                continue;
            }

            renderer.fillRectangles(
                rectangles,
                polityColor(*polity, 255)
            );
        }

        if (!politicalView)
        {
            return;
        }

        for (const PolityLabelPlacement& placement
            : cache_->labelPlacements)
        {
            const Polity* polity = world.polity(placement.polityId);

            if (
                !polity ||
                placement.tileCount == 0 ||
                polity->name().empty()
            )
            {
                continue;
            }

            const float territoryWidthPixels = static_cast<float>(
                static_cast<double>(
                    placement.maximumX - placement.minimumX + 1
                ) * tilePixelsDouble
            );

            const float naturalWidth = fontRenderer_.measureWidth(
                polity->name(),
                1.0F
            );

            const float pixelSize = std::clamp(
                territoryWidthPixels
                    * policy.labelTerritoryWidthFraction
                    / std::max(naturalWidth, 1.0F),
                policy.minimumLabelPixelSize,
                policy.maximumLabelPixelSize
            );

            const float labelWidth = fontRenderer_.measureWidth(
                polity->name(),
                pixelSize
            );

            const float centerX = static_cast<float>(
                viewportWidth * 0.5
                + (
                    static_cast<double>(placement.anchor.x) + 0.5
                    - camera.tileX()
                ) * tilePixelsDouble
            );

            const float centerY = static_cast<float>(
                viewportHeight * 0.5
                + (
                    static_cast<double>(placement.anchor.y) + 0.5
                    - camera.tileY()
                ) * tilePixelsDouble
            );

            const MapColor mapColor = polity->mapColor();
            const int luminance =
                299 * static_cast<int>(mapColor.red)
                + 587 * static_cast<int>(mapColor.green)
                + 114 * static_cast<int>(mapColor.blue);

            const bool useDarkText = luminance > 145000;
            const RenderColor shadow = useDarkText
                ? RenderColor{255, 255, 255, 210}
                : RenderColor{0, 0, 0, 225};

            const RenderColor text = useDarkText
                ? RenderColor{24, 27, 32, 255}
                : RenderColor{248, 246, 238, 255};

            const float labelX = centerX - labelWidth * 0.5F;
            const float labelY = centerY - 3.5F * pixelSize;

            fontRenderer_.drawText(
                renderer,
                polity->name(),
                labelX + 1.0F,
                labelY + 1.0F,
                pixelSize,
                shadow
            );

            fontRenderer_.drawText(
                renderer,
                polity->name(),
                labelX,
                labelY,
                pixelSize,
                text
            );
        }
    }
}
