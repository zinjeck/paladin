#include "rendering/TribalInfluenceRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldSurface.h"
#include "ui/BitmapFontRenderer.h"
#include "world/Realm.h"
#include "world/Settlement.h"
#include "world/World.h"
#include "world/territory/TerritoryMap.h"
#include "world/territory/TribalInfluenceMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <span>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    namespace
    {
        constexpr int SamplesPerTile = 2;
        constexpr int SphereColumns = 192;
        constexpr int SphereRows = 96;
        constexpr double Pi = 3.14159265358979323846;
        constexpr double TwoPi = Pi * 2.0;
        constexpr int CivicReliefDepthTiles = 5;

        struct RealmLabelPlacement
        {
            RealmId realmId;
            std::uint64_t sampleCount = 0;
            double sumLongitudeSin = 0.0;
            double sumLongitudeCos = 0.0;
            double sumY = 0.0;
            WorldTilePosition anchor;
            double anchorDistanceSquared =
                std::numeric_limits<double>::max();
        };

        struct ReliefNode
        {
            int x = 0;
            int y = 0;
        };

        std::uint64_t mixSignature(
            std::uint64_t signature,
            std::uint64_t value
        ) noexcept
        {
            signature ^= value;
            signature *= 1099511628211ULL;
            return signature;
        }

        std::uint64_t realmSignature(std::span<const Realm> realms) noexcept
        {
            std::uint64_t signature = 1469598103934665603ULL;
            for (const Realm& realm : realms)
            {
                const MapColor color = realm.mapColor();
                signature = mixSignature(signature, realm.id().value());
                signature = mixSignature(signature, color.red);
                signature = mixSignature(signature, color.green);
                signature = mixSignature(signature, color.blue);
                signature = mixSignature(
                    signature,
                    realm.usesTribalInfluence() ? 1ULL : 2ULL
                );
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

        double smoothStep(double edge0, double edge1, double value) noexcept
        {
            if (!(edge1 > edge0))
            {
                return value >= edge1 ? 1.0 : 0.0;
            }
            const double t = std::clamp(
                (value - edge0) / (edge1 - edge0),
                0.0,
                1.0
            );
            return t * t * (3.0 - 2.0 * t);
        }

        std::uint8_t byte(double value) noexcept
        {
            return static_cast<std::uint8_t>(std::clamp(
                std::lround(value),
                0L,
                255L
            ));
        }

        RenderColor tribalPixel(
            const World& world,
            const TribalInfluenceSample& sample,
            const TribalInfluencePolicy& policy
        ) noexcept
        {
            const Realm* primary = world.realm(sample.primaryRealm);
            if (!primary || !primary->usesTribalInfluence())
            {
                return {0, 0, 0, 0};
            }

            const double threshold = std::clamp(
                policy.visibleInfluenceThreshold,
                0.0,
                0.95
            );
            const double primaryInfluence = sample.primaryInfluence;
            if (primaryInfluence <= threshold)
            {
                return {0, 0, 0, 0};
            }

            const Realm* secondary = world.realm(sample.secondaryRealm);
            const double secondaryInfluence =
                secondary && secondary->usesTribalInfluence()
                    ? sample.secondaryInfluence
                    : 0.0;

            double primaryWeight = 1.0;
            if (secondaryInfluence > threshold)
            {
                primaryWeight = tribalPrimaryBlendWeight(
                    primaryInfluence,
                    secondaryInfluence,
                    policy
                );
            }

            const MapColor first = primary->mapColor();
            MapColor second = first;
            if (secondaryInfluence > threshold && secondary)
            {
                second = secondary->mapColor();
            }

            // Low-authority outskirts are subtly darker. The core approaches
            // the authored map color rather than blooming brighter than it.
            const double edgeRelief = smoothStep(
                threshold,
                0.72,
                std::max(primaryInfluence, secondaryInfluence)
            );
            const double shade = 0.84 + 0.16 * edgeRelief;

            const auto blendChannel = [&](std::uint8_t a, std::uint8_t b)
            {
                return byte(
                    (a * primaryWeight + b * (1.0 - primaryWeight)) * shade
                );
            };

            // Authority itself controls opacity, so an unopposed tribal realm
            // evaporates into the base map instead of ending at a binary edge.
            const double coverage = smoothStep(
                threshold,
                0.58,
                primaryInfluence
            );
            const double overlapCoverage =
                secondaryInfluence > threshold
                    ? smoothStep(threshold, 0.48, secondaryInfluence)
                    : 0.0;
            const double alpha =
                214.0 * std::clamp(coverage + overlapCoverage * 0.18, 0.0, 1.0);

            return {
                blendChannel(first.red, second.red),
                blendChannel(first.green, second.green),
                blendChannel(first.blue, second.blue),
                byte(alpha)
            };
        }

        RealmId wrappedCivicController(
            const TerritoryMap& territory,
            int x,
            int y
        ) noexcept
        {
            if (territory.width() <= 0 || y < 0 || y >= territory.height())
            {
                return {};
            }
            x %= territory.width();
            if (x < 0)
            {
                x += territory.width();
            }
            return territory.controllerAt({x, y});
        }

        void drawMappedFlat(
            Renderer& renderer,
            const Texture* texture,
            int textureWidth,
            int textureHeight,
            int worldWidth,
            int worldHeight,
            const Camera2D& camera,
            double tilePixels,
            float visibility
        )
        {
            if (!texture || visibility <= 0.001F || tilePixels <= 0.0)
            {
                return;
            }
            renderer.drawTexture(
                *texture,
                0.0F,
                0.0F,
                static_cast<float>(textureWidth),
                static_cast<float>(textureHeight),
                static_cast<float>(
                    renderer.outputWidth() * 0.5 - camera.tileX() * tilePixels
                ),
                static_cast<float>(
                    renderer.outputHeight() * 0.5 - camera.tileY() * tilePixels
                ),
                static_cast<float>(worldWidth * tilePixels),
                static_cast<float>(worldHeight * tilePixels),
                byte(255.0 * visibility)
            );
        }
    } // namespace


    struct TribalInfluenceRenderer::Cache
    {
        const TribalInfluenceMap* influenceSource = nullptr;
        const TerritoryMap* civicSource = nullptr;
        std::uint64_t influenceRevision =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t civicRevision =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t realmsSignature = 0;
        int worldWidth = 0;
        int worldHeight = 0;
        int textureWidth = 0;
        int textureHeight = 0;
        std::unique_ptr<Texture> tribalOverlay;
        std::unique_ptr<Texture> civicReliefOverlay;
        std::vector<RenderColor> tribalPixels;
        std::vector<RenderColor> civicReliefPixels;
        std::vector<RealmLabelPlacement> tribalLabels;
        std::vector<WorldSurface::Point3> spherePoints;
        BitmapFontRenderer fontRenderer;

        void ensureSpherePoints()
        {
            const std::size_t required =
                std::size_t(SphereColumns + 1) * (SphereRows + 1);
            if (spherePoints.size() == required)
            {
                return;
            }
            spherePoints.resize(required);
            for (int row = 0; row <= SphereRows; ++row)
            {
                for (int column = 0; column <= SphereColumns; ++column)
                {
                    spherePoints[std::size_t(row) * (SphereColumns + 1) +
                                 column] = WorldSurface::sphere(
                        double(column) / SphereColumns,
                        double(row) / SphereRows
                    );
                }
            }
        }

        void ensure(Renderer& renderer, const World& world)
        {
            const TribalInfluenceMap& influence = world.tribalInfluence();
            const TerritoryMap& civic = world.territory();
            const auto realms = world.realms();
            const std::uint64_t signature = realmSignature(realms);

            if (influenceSource == &influence && civicSource == &civic &&
                influenceRevision == influence.revision() &&
                civicRevision == civic.revision() &&
                realmsSignature == signature)
            {
                return;
            }

            influenceSource = &influence;
            civicSource = &civic;
            influenceRevision = influence.revision();
            civicRevision = civic.revision();
            realmsSignature = signature;
            worldWidth = influence.width();
            worldHeight = influence.height();
            textureWidth = worldWidth * SamplesPerTile;
            textureHeight = worldHeight * SamplesPerTile;

            const std::size_t pixelCount =
                std::size_t(textureWidth) * textureHeight;
            tribalPixels.assign(pixelCount, RenderColor{0, 0, 0, 0});
            civicReliefPixels.assign(pixelCount, RenderColor{0, 0, 0, 0});
            tribalLabels.clear();

            if (worldWidth <= 0 || worldHeight <= 0)
            {
                tribalOverlay.reset();
                civicReliefOverlay.reset();
                return;
            }

            const TribalInfluencePolicy& policy =
                world.territoryFoundationPolicy().tribalInfluence;

            for (int py = 0; py < textureHeight; ++py)
            {
                for (int px = 0; px < textureWidth; ++px)
                {
                    const double worldX =
                        (double(px) + 0.5) / SamplesPerTile;
                    const double worldY =
                        (double(py) + 0.5) / SamplesPerTile;
                    const RealmId civicController = wrappedCivicController(
                        civic,
                        static_cast<int>(std::floor(worldX)),
                        static_cast<int>(std::floor(worldY))
                    );
                    if (civicController.isValid())
                    {
                        continue;
                    }

                    const TribalInfluenceSample sample =
                        influence.sampleContinuous(worldX, worldY);
                    tribalPixels[std::size_t(py) * textureWidth + px] =
                        tribalPixel(world, sample, policy);
                }
            }

            // Build tribal label footprints from the actual field rather than
            // from legacy controller tiles. Circular longitude averaging keeps
            // labels stable for influence that crosses the world seam.
            std::unordered_map<RealmId, std::size_t, StrongIdHash> labelByRealm;
            for (const Realm& realm : realms)
            {
                if (!realm.usesTribalInfluence())
                {
                    continue;
                }
                const std::size_t index = tribalLabels.size();
                tribalLabels.push_back({});
                tribalLabels.back().realmId = realm.id();
                labelByRealm.emplace(realm.id(), index);
            }

            const double visibleThreshold = std::max(
                0.0,
                policy.visibleInfluenceThreshold
            );
            for (int y = 0; y < worldHeight; ++y)
            {
                for (int x = 0; x < worldWidth; ++x)
                {
                    if (civic.controllerAt({x, y}).isValid())
                    {
                        continue;
                    }
                    const TribalInfluenceSample sample = influence.sampleAt({x, y});
                    if (sample.primaryInfluence <= visibleThreshold)
                    {
                        continue;
                    }
                    const auto it = labelByRealm.find(sample.primaryRealm);
                    if (it == labelByRealm.end())
                    {
                        continue;
                    }
                    RealmLabelPlacement& placement = tribalLabels[it->second];
                    const double longitude =
                        (double(x) + 0.5) / worldWidth * TwoPi;
                    ++placement.sampleCount;
                    placement.sumLongitudeSin += std::sin(longitude);
                    placement.sumLongitudeCos += std::cos(longitude);
                    placement.sumY += double(y) + 0.5;
                }
            }

            for (RealmLabelPlacement& placement : tribalLabels)
            {
                if (placement.sampleCount == 0)
                {
                    continue;
                }
                double longitude = std::atan2(
                    placement.sumLongitudeSin,
                    placement.sumLongitudeCos
                );
                if (longitude < 0.0)
                {
                    longitude += TwoPi;
                }
                const double centerX = longitude / TwoPi * worldWidth;
                const double centerY =
                    placement.sumY / double(placement.sampleCount);
                for (int y = 0; y < worldHeight; ++y)
                {
                    for (int x = 0; x < worldWidth; ++x)
                    {
                        if (civic.controllerAt({x, y}).isValid())
                        {
                            continue;
                        }
                        const TribalInfluenceSample sample =
                            influence.sampleAt({x, y});
                        if (sample.primaryRealm != placement.realmId ||
                            sample.primaryInfluence <= visibleThreshold)
                        {
                            continue;
                        }
                        double dx =
                            std::abs((double(x) + 0.5) - centerX);
                        dx = std::min(dx, double(worldWidth) - dx);
                        const double dy = double(y) + 0.5 - centerY;
                        const double distanceSquared = dx * dx + dy * dy;
                        if (distanceSquared < placement.anchorDistanceSquared)
                        {
                            placement.anchorDistanceSquared = distanceSquared;
                            placement.anchor = {x, y};
                        }
                    }
                }
            }

            // Civic relief is a purely visual inward distance field at half-tile
            // resolution. It gives established states a gentle dark rim while
            // preserving TerritoryMap as the sole civic sovereignty source.
            const int reliefWidth = textureWidth;
            const int reliefHeight = textureHeight;
            std::vector<RealmId> reliefOwners(pixelCount);
            std::vector<int> reliefDistance(pixelCount, -1);
            std::queue<ReliefNode> frontier;

            const auto reliefIndex = [reliefWidth](int x, int y)
            {
                return std::size_t(y) * reliefWidth + x;
            };
            const auto reliefOwner = [&](int x, int y) -> RealmId
            {
                if (y < 0 || y >= reliefHeight)
                {
                    return {};
                }
                x %= reliefWidth;
                if (x < 0)
                {
                    x += reliefWidth;
                }
                return reliefOwners[reliefIndex(x, y)];
            };

            for (int y = 0; y < reliefHeight; ++y)
            {
                for (int x = 0; x < reliefWidth; ++x)
                {
                    reliefOwners[reliefIndex(x, y)] = wrappedCivicController(
                        civic,
                        x / SamplesPerTile,
                        y / SamplesPerTile
                    );
                }
            }

            constexpr std::array<std::array<int, 2>, 4> Cardinal{
                std::array<int, 2>{-1, 0},
                std::array<int, 2>{1, 0},
                std::array<int, 2>{0, -1},
                std::array<int, 2>{0, 1}
            };
            for (int y = 0; y < reliefHeight; ++y)
            {
                for (int x = 0; x < reliefWidth; ++x)
                {
                    const RealmId owner = reliefOwners[reliefIndex(x, y)];
                    if (!owner.isValid())
                    {
                        continue;
                    }
                    bool boundary = false;
                    for (const auto offset : Cardinal)
                    {
                        if (reliefOwner(x + offset[0], y + offset[1]) != owner)
                        {
                            boundary = true;
                            break;
                        }
                    }
                    if (boundary)
                    {
                        reliefDistance[reliefIndex(x, y)] = 0;
                        frontier.push({x, y});
                    }
                }
            }

            const int maximumReliefDistance =
                CivicReliefDepthTiles * SamplesPerTile;
            while (!frontier.empty())
            {
                const ReliefNode node = frontier.front();
                frontier.pop();
                const std::size_t nodeIndex = reliefIndex(node.x, node.y);
                const int distance = reliefDistance[nodeIndex];
                if (distance >= maximumReliefDistance)
                {
                    continue;
                }
                const RealmId owner = reliefOwners[nodeIndex];
                for (const auto offset : Cardinal)
                {
                    int nx = node.x + offset[0];
                    const int ny = node.y + offset[1];
                    if (ny < 0 || ny >= reliefHeight)
                    {
                        continue;
                    }
                    nx %= reliefWidth;
                    if (nx < 0)
                    {
                        nx += reliefWidth;
                    }
                    const std::size_t nextIndex = reliefIndex(nx, ny);
                    if (reliefOwners[nextIndex] != owner ||
                        reliefDistance[nextIndex] >= 0)
                    {
                        continue;
                    }
                    reliefDistance[nextIndex] = distance + 1;
                    frontier.push({nx, ny});
                }
            }

            for (std::size_t index = 0; index < pixelCount; ++index)
            {
                const int distance = reliefDistance[index];
                if (distance < 0 || distance > maximumReliefDistance)
                {
                    continue;
                }
                const double t = 1.0 -
                    double(distance) / std::max(1, maximumReliefDistance);
                // Maximum 14% opacity: intentionally gentler than CK-style
                // beveling and closer to Victoria II's quiet territorial body.
                const std::uint8_t alpha = byte(36.0 * t * t);
                civicReliefPixels[index] = {8, 15, 27, alpha};
            }

            const auto upload = [&](std::unique_ptr<Texture>& texture,
                                    const std::vector<RenderColor>& pixels)
            {
                bool updated = false;
                if (texture && texture->width() == textureWidth &&
                    texture->height() == textureHeight)
                {
                    updated = renderer.updateTexturePixels(*texture, pixels);
                }
                if (!updated)
                {
                    texture = renderer.createTextureFromPixels(
                        textureWidth,
                        textureHeight,
                        pixels
                    );
                }
                if (texture)
                {
                    renderer.setTextureFiltering(*texture, false);
                }
            };

            upload(tribalOverlay, tribalPixels);
            upload(civicReliefOverlay, civicReliefPixels);
        }

        void drawTextureOnGlobe(
            Renderer& renderer,
            const Texture* texture,
            const World& world,
            const Camera2D& camera,
            float visibility
        )
        {
            if (!texture || visibility <= 0.001F)
            {
                return;
            }

            ensureSpherePoints();
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            std::vector<MeshVertex> vertices(spherePoints.size());
            std::vector<double> depths(spherePoints.size());
            const RenderColor vertexColor{
                255,
                255,
                255,
                byte(255.0 * visibility)
            };

            for (int row = 0; row <= SphereRows; ++row)
            {
                for (int column = 0; column <= SphereColumns; ++column)
                {
                    const std::size_t index =
                        std::size_t(row) * (SphereColumns + 1) + column;
                    const auto point = view.orient(spherePoints[index]);
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
                    if (depths[a] > 0.0 && depths[b] > 0.0 && depths[c] > 0.0)
                    {
                        indices.push_back(a);
                        indices.push_back(b);
                        indices.push_back(c);
                    }
                    if (depths[a] > 0.0 && depths[c] > 0.0 && depths[d] > 0.0)
                    {
                        indices.push_back(a);
                        indices.push_back(c);
                        indices.push_back(d);
                    }
                }
            }
            if (!indices.empty())
            {
                renderer.drawMesh(*texture, vertices, indices);
            }
        }

        void drawTribalLabelsFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            double tilePixels,
            float visibility,
            const WorldPresentationPolicy& policy
        )
        {
            if (visibility <= 0.001F)
            {
                return;
            }
            for (const RealmLabelPlacement& placement : tribalLabels)
            {
                const Realm* realm = world.realm(placement.realmId);
                if (!realm || placement.sampleCount == 0 || realm->name().empty())
                {
                    continue;
                }
                const double representativeWidthTiles = std::max(
                    1.0,
                    std::sqrt(double(placement.sampleCount)) * 1.8
                );
                const float naturalWidth =
                    fontRenderer.measureWidth(realm->name(), 1.0F);
                const float pixelSize = std::clamp(
                    float(representativeWidthTiles * tilePixels) *
                        policy.realmLabelTerritoryWidthFraction /
                        std::max(naturalWidth, 1.0F),
                    std::max(0.5F, policy.minimumRealmLabelPixelSize),
                    std::max(
                        std::max(0.5F, policy.minimumRealmLabelPixelSize),
                        policy.maximumRealmLabelPixelSize
                    )
                );
                const float labelWidth =
                    fontRenderer.measureWidth(realm->name(), pixelSize);
                const float centerX = float(
                    renderer.outputWidth() * 0.5 +
                    (double(placement.anchor.x) + 0.5 - camera.tileX()) *
                        tilePixels
                );
                const float centerY = float(
                    renderer.outputHeight() * 0.5 +
                    (double(placement.anchor.y) + 0.5 - camera.tileY()) *
                        tilePixels
                );
                const MapColor mapColor = realm->mapColor();
                const int luminance = 299 * int(mapColor.red) +
                                      587 * int(mapColor.green) +
                                      114 * int(mapColor.blue);
                const bool darkText = luminance > 145000;
                RenderColor shadow = darkText
                                         ? RenderColor{255, 255, 255, 210}
                                         : RenderColor{8, 15, 27, 225};
                RenderColor text = darkText
                                       ? RenderColor{24, 27, 32, 255}
                                       : RenderColor{244, 243, 232, 255};
                shadow.alpha = byte(shadow.alpha * visibility);
                text.alpha = byte(text.alpha * visibility);
                const float x = centerX - labelWidth * 0.5F;
                const float y = centerY - 3.5F * pixelSize;
                fontRenderer.drawText(
                    renderer,
                    realm->name(),
                    x + 1.0F,
                    y + 1.0F,
                    pixelSize,
                    shadow
                );
                fontRenderer.drawText(
                    renderer,
                    realm->name(),
                    x,
                    y,
                    pixelSize,
                    text
                );
            }
        }

        void drawTribalLabelsGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            double tilePixels,
            float visibility,
            const WorldPresentationPolicy& policy
        )
        {
            if (visibility <= 0.001F)
            {
                return;
            }
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            for (const RealmLabelPlacement& placement : tribalLabels)
            {
                const Realm* realm = world.realm(placement.realmId);
                if (!realm || placement.sampleCount == 0 || realm->name().empty())
                {
                    continue;
                }
                const auto projected = view.project(
                    (double(placement.anchor.x) + 0.5) / worldWidth,
                    (double(placement.anchor.y) + 0.5) / worldHeight
                );
                if (projected.z <= 0.15)
                {
                    continue;
                }

                const double representativeWidthTiles = std::max(
                    1.0,
                    std::sqrt(double(placement.sampleCount)) * 1.8
                );
                const float naturalWidth =
                    fontRenderer.measureWidth(realm->name(), 1.0F);
                const float pixelSize = std::clamp(
                    float(representativeWidthTiles * tilePixels) *
                        policy.realmLabelTerritoryWidthFraction /
                        std::max(naturalWidth, 1.0F),
                    std::max(0.5F, policy.minimumRealmLabelPixelSize),
                    std::max(
                        std::max(0.5F, policy.minimumRealmLabelPixelSize),
                        policy.maximumRealmLabelPixelSize
                    )
                );
                const float labelWidth =
                    fontRenderer.measureWidth(realm->name(), pixelSize);
                const MapColor mapColor = realm->mapColor();
                const int luminance = 299 * int(mapColor.red) +
                                      587 * int(mapColor.green) +
                                      114 * int(mapColor.blue);
                const bool darkText = luminance > 145000;
                const float projectedVisibility = float(
                    std::clamp(projected.z * 4.0, 0.0, 1.0) * visibility
                );
                RenderColor shadow = darkText
                                         ? RenderColor{255, 255, 255, 210}
                                         : RenderColor{8, 15, 27, 225};
                RenderColor text = darkText
                                       ? RenderColor{24, 27, 32, 255}
                                       : RenderColor{244, 243, 232, 255};
                shadow.alpha = byte(shadow.alpha * projectedVisibility);
                text.alpha = byte(text.alpha * projectedVisibility);
                const float x = float(projected.x) - labelWidth * 0.5F;
                const float y = float(projected.y) - 3.5F * pixelSize;
                fontRenderer.drawText(
                    renderer,
                    realm->name(),
                    x + 1.0F,
                    y + 1.0F,
                    pixelSize,
                    shadow
                );
                fontRenderer.drawText(
                    renderer,
                    realm->name(),
                    x,
                    y,
                    pixelSize,
                    text
                );
            }
        }
    };


    TribalInfluenceRenderer::TribalInfluenceRenderer()
        : cache_(std::make_unique<Cache>())
    {
    }


    TribalInfluenceRenderer::~TribalInfluenceRenderer() = default;


    void TribalInfluenceRenderer::reset()
    {
        cache_ = std::make_unique<Cache>();
    }


    void TribalInfluenceRenderer::renderTribalFlat(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const WorldPresentationState& presentation,
        const WorldPresentationPolicy& policy
    )
    {
        cache_->ensure(renderer, world);
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        drawMappedFlat(
            renderer,
            cache_->tribalOverlay.get(),
            cache_->textureWidth,
            cache_->textureHeight,
            cache_->worldWidth,
            cache_->worldHeight,
            camera,
            tilePixels,
            presentation.realmFillWeight
        );
        cache_->drawTribalLabelsFlat(
            renderer,
            world,
            camera,
            tilePixels,
            presentation.realmLabelWeight,
            policy
        );
    }


    void TribalInfluenceRenderer::renderTribalGlobe(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const WorldPresentationState& presentation,
        const WorldPresentationPolicy& policy
    )
    {
        cache_->ensure(renderer, world);
        cache_->drawTextureOnGlobe(
            renderer,
            cache_->tribalOverlay.get(),
            world,
            camera,
            presentation.realmFillWeight
        );
        const auto view = GlobeView::from(
            camera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight()
        );
        const double tilePixels =
            view.radius * TwoPi / std::max(1, world.grid().width());
        cache_->drawTribalLabelsGlobe(
            renderer,
            world,
            camera,
            tilePixels,
            presentation.realmLabelWeight,
            policy
        );
    }


    void TribalInfluenceRenderer::renderCivicReliefFlat(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const WorldPresentationState& presentation
    )
    {
        cache_->ensure(renderer, world);
        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        drawMappedFlat(
            renderer,
            cache_->civicReliefOverlay.get(),
            cache_->textureWidth,
            cache_->textureHeight,
            cache_->worldWidth,
            cache_->worldHeight,
            camera,
            tilePixels,
            presentation.realmFillWeight
        );
    }


    void TribalInfluenceRenderer::renderCivicReliefGlobe(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const WorldPresentationState& presentation
    )
    {
        cache_->ensure(renderer, world);
        cache_->drawTextureOnGlobe(
            renderer,
            cache_->civicReliefOverlay.get(),
            world,
            camera,
            presentation.realmFillWeight
        );
    }
} // namespace Paladin
