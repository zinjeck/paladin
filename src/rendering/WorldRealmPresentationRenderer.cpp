#include "rendering/WorldRealmPresentationRenderer.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldPixelGrid.h"
#include "rendering/WorldPoliticalSurface.h"
#include "rendering/WorldRealmQuery.h"
#include "rendering/WorldThematicPalette.h"
#include "world/WorldPopulationField.h"
#include "ui/BitmapFontRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace Paladin
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;
        constexpr int ChunkSide = 16;
        constexpr std::size_t MaximumDetailChunks = 42; // <= 31.5 MiB including selection masks
        double smoothStep(double edge0, double edge1, double value) noexcept
        {
            if (!(edge1 > edge0))
            {
                return value >= edge1 ? 1.0 : 0.0;
            }
            const double t =
                std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

        std::uint8_t byte(double value) noexcept
        {
            return static_cast<std::uint8_t>(
                std::clamp(std::lround(value), 0L, 255L)
            );
        }

        struct RealmInk
        {
            MapColor color;
            bool tribal = false;
        };
        using PoliticalPalette =
            std::unordered_map<RealmId, RealmInk, StrongIdHash>;

        RenderColor tribalPixel(
            const PoliticalPalette& palette,
            const TribalInfluenceSample& sample,
            const TribalInfluencePolicy& policy
        ) noexcept
        {
            const auto primary = palette.find(sample.primaryRealm);
            if (primary == palette.end() || !primary->second.tribal)
            {
                return {0, 0, 0, 0};
            }

            const double threshold =
                std::clamp(policy.visibleInfluenceThreshold, 0.0, 0.95);
            const double primaryInfluence = sample.primaryInfluence;
            if (primaryInfluence <= threshold)
            {
                return {0, 0, 0, 0};
            }

            const auto secondary = palette.find(sample.secondaryRealm);
            const double secondaryInfluence =
                secondary != palette.end() && secondary->second.tribal
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

            const MapColor first = primary->second.color;
            MapColor second = first;
            if (secondaryInfluence > threshold && secondary != palette.end())
            {
                second = secondary->second.color;
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
            const double coverage =
                smoothStep(threshold, 0.58, primaryInfluence);
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


        struct FrontierPixel { int x, y; RealmId realm; };
        struct PoliticalTextures
        {
            int density=1;
            std::vector<FrontierPixel> frontiers;
            std::unique_ptr<Texture> fill, border, selectedBorder;
            RealmId selectedFor;
            int x = 0, y = 0, width = 0, height = 0;
            std::uint64_t used = 0;
            std::size_t bytes = 0;
        };
        struct Label
        {
            RealmId id;
            std::size_t count = 0;
            double sine = 0, cosine = 0, sumY = 0;
            double x = 0, y = 0, best = std::numeric_limits<double>::max();
            WorldTilePosition anchor{};
        };

        // Border extraction only needs the preceding, current and following
        // sample row. Keep that exact halo, rather than allocating an entire
        // world-sized array of the relatively large surface records.
        struct PoliticalRaster
        {
            int x, y, width, height, density, pw, ph, stride;
            int nextSample = -1, row = 0, uploadLayer = 0, uploadRow = 0;
            std::vector<WorldPoliticalSurfaceSample> samples;
            std::vector<RealmId> regions;
            bool recordFrontiers=false;
            std::vector<RenderColor> fills, borders;
            bool hasFill = false, hasBorder = false;
            PoliticalTextures result;

            PoliticalRaster(int tx, int ty, int w, int h, int d, bool record=false)
                : x(tx), y(ty), width(w), height(h), density(d), pw(w * d),
                  ph(h * d), stride(pw + 2), samples(std::size_t(stride) * 3), regions(std::size_t(stride)*3), recordFrontiers(record)
            {
                // Reserve address space once, then touch only one output row
                // per work slice. Large initial zero-fills otherwise stall too.
                fills.reserve(std::size_t(pw) * ph);
                borders.reserve(std::size_t(pw) * ph);
                result.density=d;
                result.x = x;
                result.y = y;
                result.width = width;
                result.height = height;
            }
        };

        struct PoliticalPreparation
        {
            int stage = 0, row = 0;
            std::vector<RealmId> owners, labelOwners;
            std::vector<float> distance;
            std::vector<Label> labels;
            std::unordered_map<RealmId, std::size_t, StrongIdHash> byRealm;
            std::queue<WorldTilePosition> frontier;
            std::unique_ptr<PoliticalRaster> raster;
            std::shared_ptr<const WorldTribalSurfaceSource> influence;
            std::shared_ptr<WorldPoliticalSurfaceSource> surface;
            PoliticalPalette palette;
            std::shared_ptr<const WorldPopulationField> population;
        };

        struct DetailRequest
        {
            int x, y;
            double priority;
        };
    } // namespace

    struct WorldRealmPresentationRenderer::Cache
    {
        const World* source = nullptr;
        const Renderer* rendererOwner = nullptr;
        std::uint64_t signature = 0, topologySignature = 0, builds = 0,
                      frame = 0;
        std::uint64_t budgetFrame = std::numeric_limits<std::uint64_t>::max();
        std::chrono::steady_clock::time_point workDeadline{};
        std::chrono::nanoseconds remainingWork{};
        bool ready = false;
        bool detailDemandPending = false;
        int width = 0, height = 0;
        WorldMapMode mode=WorldMapMode::Political;
        RealmId selected;
        std::uint64_t coarseGeneration=0;
        PoliticalTextures coarse;
        std::unordered_map<std::uint64_t, PoliticalTextures> detail;
        std::vector<float> distance;
        std::vector<Label> labels;
        std::vector<MeshVertex> vertices;
        std::vector<int> indices;
        BitmapFontRenderer font;
        std::unique_ptr<PoliticalPreparation> preparation;
        std::unique_ptr<PoliticalRaster> detailRaster;
        std::shared_ptr<const WorldTribalSurfaceSource> presentedInfluence;
        std::shared_ptr<WorldPoliticalSurfaceSource> surfaceSource;
        PoliticalPalette presentedPalette;
        std::shared_ptr<const WorldPopulationField> presentedPopulation;
        std::vector<DetailRequest> previousDemand, currentDemand;

        struct WorkSlice
        {
            Cache& cache;
            std::chrono::steady_clock::time_point started;
            explicit WorkSlice(Cache& owner)
                : cache(owner), started(std::chrono::steady_clock::now())
            {
                cache.workDeadline = started + cache.remainingWork;
            }
            ~WorkSlice()
            {
                const auto used =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started
                    );
                cache.remainingWork = std::max(
                    std::chrono::nanoseconds::zero(),
                    cache.remainingWork - used
                );
            }
        };

        static std::uint64_t key(int x, int y)
        {
            return (std::uint64_t(std::uint32_t(y)) << 32) | std::uint32_t(x);
        }

        void beginWorkFrame(const Renderer& renderer, bool loading = false)
        {
            if (budgetFrame == renderer.frameId())
            {
                return;
            }
            budgetFrame = renderer.frameId();
            ++frame;
            detailDemandPending = false;
            previousDemand.swap(currentDemand);
            currentDemand.clear();
            const auto previousWanted = selectedDemand(previousDemand);
            if (detailRaster && std::find(
                                    previousWanted.begin(),
                                    previousWanted.end(),
                                    std::pair{detailRaster->x, detailRaster->y}
                                ) == previousWanted.end())
            {
                detailRaster.reset();
            }
            remainingWork = std::chrono::milliseconds(loading ? 8 : 2);
        }

        bool timeRemaining() const
        {
            return std::chrono::steady_clock::now() < workDeadline;
        }

        void ensure(Renderer& renderer, const World& world)
        {
            const auto& influence = world.tribalInfluence();
            std::uint64_t next = 1469598103934665603ULL;
            const auto mix = [&](std::uint64_t value)
            { next = (next ^ value) * 1099511628211ULL; };
            mix(std::uint64_t(mode));
            mix(world.grid().revision());
            mix(world.territory().revision());
            mix(world.grid().width());
            mix(world.grid().height());
            for (const auto& realm : world.realms())
            {
                mix(realm.id().value());
                mix(realm.usesTribalInfluence());
            }
            const auto topology = next;
            if (mode==WorldMapMode::Population) mix(WorldPopulationField::fingerprint(world));
            mix(influence.revision());
            for (const auto& realm : world.realms())
            {
                const auto color = realm.mapColor();
                mix(color.red);
                mix(color.green);
                mix(color.blue);
                for (unsigned char ch : realm.name())
                {
                    mix(ch);
                }
            }
            if (source == &world && rendererOwner == &renderer &&
                next == signature)
            {
                return;
            }
            const bool topologyChanged = source != &world ||
                                         rendererOwner != &renderer ||
                                         topology != topologySignature;
            // Finish the immutable requested revision even when residents keep
            // changing. Restarting on every tick could starve a sliced refresh.
            if (preparation && !topologyChanged)
            {
                return;
            }
            source = &world;
            rendererOwner = &renderer;
            signature = next;
            topologySignature = topology;
            width = world.grid().width();
            height = world.grid().height();
            detailRaster.reset();
            previousDemand.clear();
            currentDemand.clear();
            // Influence and palette refreshes are transactional: the last exact
            // completed presentation remains visible. A coast/controller change
            // must discard old ink immediately, since it could now cover water.
            if (topologyChanged)
            {
                ready = false;
                coarse = {};
                detail.clear();
                labels.clear();
                distance.clear();
                presentedInfluence.reset();
                presentedPalette.clear();
            }
            preparation = std::make_unique<PoliticalPreparation>();
            if (width <= 0 || height <= 0)
            {
                preparation.reset();
                ready = true;
                return;
            }
            const auto count = std::size_t(width) * height;
            preparation->owners.resize(count);
            preparation->labelOwners.resize(count);
            preparation->distance.assign(count, 5.F);
            preparation->influence =
                std::make_shared<WorldTribalSurfaceSource>(influence);
            if (topologyChanged || !surfaceSource)
            {
                surfaceSource =
                    std::make_shared<WorldPoliticalSurfaceSource>(world);
            }
            preparation->surface = surfaceSource;
            if (!surfaceSource->complete())
            {
                preparation->stage = -1;
            }
            if (mode==WorldMapMode::Population) preparation->population=std::make_shared<WorldPopulationField>(world);
            for (const auto& realm : world.realms())
            {
                preparation->byRealm[realm.id()] = preparation->labels.size();
                preparation->labels.push_back({realm.id()});
                preparation->palette.emplace(
                    realm.id(),
                    RealmInk{[&] {
                        if (!thematicMapMode(mode)) return realm.mapColor();
                        const auto c=mode==WorldMapMode::Government ?
                            (realm.usesTribalInfluence()?GovernmentTribal:GovernmentCivic) : populationMapColor(0);
                        return MapColor{c.red,c.green,c.blue};
                    }(), realm.usesTribalInfluence()}
                );
            }
        }

        void advancePreparation(Renderer& renderer, const World& world)
        {
            if (!preparation)
            {
                return;
            }
            auto& job = *preparation;
            const auto& influence = *job.influence;
            const auto index = [&](int x, int y)
            { return std::size_t(y) * width + (x % width + width) % width; };
            const auto owner = [&](int x, int y) -> RealmId
            {
                return y < 0 || y >= height ? RealmId{}
                                            : job.owners[index(x, y)];
            };
            constexpr std::array<WorldTilePosition, 4> offsets{
                {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}
            };
            const double threshold =
                world.territoryFoundationPolicy()
                    .tribalInfluence.visibleInfluenceThreshold;
            while (timeRemaining())
            {
                if (job.stage == -1)
                {
                    job.surface->appendRow(world);
                    if (job.surface->complete())
                    {
                        ++job.stage;
                    }
                }
                else if (job.stage == 0)
                {
                    const int y = job.row++;
                    for (int x = 0; x < width; ++x)
                    {
                        const auto surface = worldPoliticalSurfaceAt(
                            *job.surface,
                            x + .5,
                            y + .5
                        );
                        job.owners[index(x, y)] = surface.civic;
                        RealmId id = surface.civic;
                        if (surface.land && !id)
                        {
                            const auto t = influence.sampleAt({x, y});
                            if (t.primaryInfluence > threshold)
                            {
                                id = t.primaryRealm;
                            }
                        }
                        job.labelOwners[index(x, y)] = id;
                        const auto it = job.byRealm.find(id);
                        if (it == job.byRealm.end())
                        {
                            continue;
                        }
                        auto& l = job.labels[it->second];
                        const double longitude = (x + .5) / width * 2 * Pi;
                        ++l.count;
                        l.sine += std::sin(longitude);
                        l.cosine += std::cos(longitude);
                        l.sumY += y + .5;
                    }
                    if (job.row == height)
                    {
                        ++job.stage;
                        job.row = 0;
                    }
                }
                else if (job.stage == 1)
                {
                    const int y = job.row++;
                    for (int x = 0; x < width; ++x)
                    {
                        const auto id = owner(x, y);
                        if (!id)
                        {
                            job.distance[index(x, y)] = 0;
                            continue;
                        }
                        for (auto o : offsets)
                        {
                            if (owner(x + o.x, y + o.y) != id)
                            {
                                job.distance[index(x, y)] = .5F;
                                job.frontier.push({x, y});
                                break;
                            }
                        }
                    }
                    if (job.row == height)
                    {
                        ++job.stage;
                        job.row = 0;
                    }
                }
                else if (job.stage == 2)
                {
                    for (int n = 0; n < 64 && !job.frontier.empty(); ++n)
                    {
                        const auto p = job.frontier.front();
                        job.frontier.pop();
                        const float d = job.distance[index(p.x, p.y)] + 1;
                        if (d >= 5)
                        {
                            continue;
                        }
                        for (auto o : offsets)
                        {
                            const int x = (p.x + o.x + width) % width,
                                      y = p.y + o.y;
                            if (y < 0 || y >= height ||
                                owner(x, y) != owner(p.x, p.y))
                            {
                                continue;
                            }
                            if (job.distance[index(x, y)] > d)
                            {
                                job.distance[index(x, y)] = d;
                                job.frontier.push({x, y});
                            }
                        }
                    }
                    if (job.frontier.empty())
                    {
                        ++job.stage;
                    }
                }
                else if (job.stage == 3)
                {
                    for (auto& l : job.labels)
                    {
                        if (l.count)
                        {
                            double angle = std::atan2(l.sine, l.cosine);
                            if (angle < 0)
                            {
                                angle += 2 * Pi;
                            }
                            l.x = angle / (2 * Pi) * width;
                            l.y = l.sumY / l.count;
                        }
                    }
                    ++job.stage;
                }
                else if (job.stage == 4)
                {
                    const int y = job.row++;
                    for (int x = 0; x < width; ++x)
                    {
                        const auto it =
                            job.byRealm.find(job.labelOwners[index(x, y)]);
                        if (it == job.byRealm.end())
                        {
                            continue;
                        }
                        auto& l = job.labels[it->second];
                        double dx = std::abs(x + .5 - l.x);
                        dx = std::min(dx, width - dx);
                        const double d =
                            dx * dx + (y + .5 - l.y) * (y + .5 - l.y);
                        if (d < l.best)
                        {
                            l.best = d;
                            l.anchor = {x, y};
                        }
                    }
                    if (job.row == height)
                    {
                        const int density =
                            std::clamp(2048 / std::max(width, height), 1, 4);
                        job.raster = std::make_unique<PoliticalRaster>(
                            0,
                            0,
                            width,
                            height,
                            density, true
                        );
                        ++job.stage;
                    }
                }
                else
                {
                    if (!advanceRaster(
                            renderer,
                            world,
                            *job.surface,
                            influence,
                            job.palette,
                            job.distance,
                            *job.raster, job.population.get()
                        ))
                    {
                        return;
                    }
                    coarse = std::move(job.raster->result);
                    ++coarseGeneration;
                    distance = std::move(job.distance);
                    labels = std::move(job.labels);
                    presentedInfluence = std::move(job.influence);
                    presentedPalette = std::move(job.palette);
                    presentedPopulation=std::move(job.population);
                    detail.clear();
                    detailRaster.reset();
                    ready = true;
                    preparation.reset();
                    ++builds;
                    return;
                }
            }
        }

        float relief(
            const WorldPoliticalSurfaceSample& s,
            const std::vector<float>& field
        ) const
        {
            double sum = 0;
            for (int i = 0; i < 4; ++i)
            {
                sum += s.dryWeights[i] *
                       field
                           [std::size_t(s.positions[i].y) * width +
                            s.positions[i].x];
            }
            return float(sum / std::max(.000001, s.landWeight));
        }

        bool advanceRaster(
            Renderer& renderer,
            const World& world,
            const WorldPoliticalSurfaceSource& surfaceSource,
            const WorldTribalSurfaceSource& influence,
            const PoliticalPalette& palette,
            const std::vector<float>& field,
            PoliticalRaster& job, const WorldPopulationField* population
        )
        {
            const auto& policy =
                world.territoryFoundationPolicy().tribalInfluence;
            while (timeRemaining())
            {
                if (job.row < job.ph)
                {
                    if (job.nextSample <= job.row + 1)
                    {
                        const int j = job.nextSample++;
                        const auto start =
                            std::size_t((j + 1) % 3) * job.stride;
                        for (int i = -1; i <= job.pw; ++i)
                        {
                            job.samples[start + i + 1] =
                                worldPoliticalSurfaceAt(
                                    surfaceSource,
                                    job.x + (i + .5) / job.density,
                                    job.y + (j + .5) / job.density
                                );
                            if (job.recordFrontiers || thematicMapMode(mode))
                                job.regions[start+i+1]=worldSurfaceRealm(job.samples[start+i+1],influence,policy.visibleInfluenceThreshold);
                        }
                        continue;
                    }
                    const int j = job.row++;
                    const auto above = std::size_t(j % 3) * job.stride;
                    const auto center = std::size_t((j + 1) % 3) * job.stride;
                    const auto below = std::size_t((j + 2) % 3) * job.stride;
                    const auto output = std::size_t(j) * job.pw;
                    job.fills.resize(output + job.pw, {0, 0, 0, 0});
                    job.borders.resize(output + job.pw, {0, 0, 0, 0});
                    for (int i = 0; i < job.pw; ++i)
                    {
                        const auto n = center + i + 1, out = output + i;
                        const auto& surface = job.samples[n];
                        const auto region=job.regions[n];
                        const bool frontier=surface.land && region &&
                            (job.regions[n-1]!=region || job.regions[n+1]!=region ||
                             job.regions[above+i+1]!=region || job.regions[below+i+1]!=region);
                        if (job.recordFrontiers && frontier) job.result.frontiers.push_back({i,j,region});
                        if (mode==WorldMapMode::Population)
                        {
                            // Density is geographic, not a realm-treasury-like
                            // statistic painted across every owned tile. The
                            // terrain below remains visible even in wilderness.
                            if (surface.land)
                            {
                                double people=0;
                                if (population) for(int k=0;k<4;++k)
                                    people+=surface.dryWeights[k]*population->at(surface.positions[k]);
                                people/=std::max(.000001,surface.landWeight);
                                auto color=populationDensityColor(people);
                                job.fills[out]=color; job.hasFill=true;
                                if(frontier) { job.borders[out]={89,102,121,110}; job.hasBorder=true; }
                            }
                            else { job.fills[out]=PopulationWater; job.hasFill=true; }
                            continue;
                        }
                        if (mode==WorldMapMode::Government)
                        {
                            RenderColor color=!surface.land?ThematicWater:UnclaimedLand;
                            if (surface.land)
                                if (const auto ink=palette.find(region);ink!=palette.end())
                                    color={ink->second.color.red,ink->second.color.green,ink->second.color.blue,255};
                            job.fills[out]=color; job.hasFill=true;
                            // Opaque, unshaded thematic fills suppress terrain and
                            // foliage on BOTH projections, including unclaimed land.
                            if (frontier) { job.borders[out]={8,15,27,230}; job.hasBorder=true; }
                            continue;
                        }
                        if (!surface.land)
                        {
                            continue;
                        }
                        if (const auto realm = palette.find(surface.civic);
                            realm != palette.end())
                        {
                            const auto c = realm->second.color;
                            const bool edge =
                                job.samples[n - 1].civic != surface.civic ||
                                job.samples[n + 1].civic != surface.civic ||
                                job.samples[above + i + 1].civic !=
                                    surface.civic ||
                                job.samples[below + i + 1].civic !=
                                    surface.civic;
                            const double shade =
                                .86 +
                                .14 * smoothStep(
                                          0,
                                          5,
                                          edge ? 0 : relief(surface, field)
                                      );
                            job.fills[out] = {
                                byte(c.red * shade),
                                byte(c.green * shade),
                                byte(c.blue * shade),
                                255
                            };
                            if (edge)
                            {
                                job.borders[out] = {
                                    byte(c.red * .52),
                                    byte(c.green * .52),
                                    byte(c.blue * .52),
                                    255
                                };
                                job.hasBorder = true;
                            }
                        }
                        else
                        {
                            job.fills[out] = tribalPixel(
                                palette,
                                worldTribalSurfaceSample(influence, surface),
                                policy
                            );
                        }
                        job.hasFill |= job.fills[out].alpha != 0;
                    }
                    continue;
                }
                if (job.uploadLayer == 2)
                {
                    job.result.used = frame;
                    job.result.bytes =
                        std::size_t(job.pw) * job.ph * 4 *
                        (bool(job.result.fill) + bool(job.result.border));
                    return true;
                }
                auto& texture =
                    job.uploadLayer == 0 ? job.result.fill : job.result.border;
                const bool hasPixels =
                    job.uploadLayer == 0 ? job.hasFill : job.hasBorder;
                if (!hasPixels)
                {
                    ++job.uploadLayer;
                    continue;
                }
                if (!texture)
                {
                    texture = renderer.createEmptyTexture(job.pw, job.ph);
                    if (!texture)
                    {
                        throw std::runtime_error(
                            "Political surface cache allocation failed"
                        );
                    }
                    renderer.setTextureFiltering(*texture, false);
                    continue;
                }
                const auto& pixels =
                    job.uploadLayer == 0 ? job.fills : job.borders;
                // Bound uploads as well as CPU sampling; a complete coarse
                // texture can contain millions of pixels on a large world.
                const int rows = std::min(16, job.ph - job.uploadRow);
                if (!renderer.updateTextureRegion(
                        *texture,
                        0,
                        job.uploadRow,
                        job.pw,
                        rows,
                        std::span(pixels).subspan(
                            std::size_t(job.uploadRow) * job.pw,
                            std::size_t(rows) * job.pw
                        )
                    ))
                {
                    throw std::runtime_error(
                        "Political surface cache upload failed"
                    );
                }
                job.uploadRow += rows;
                if (job.uploadRow == job.ph)
                {
                    ++job.uploadLayer;
                    job.uploadRow = 0;
                }
            }
            return false;
        }

        static std::vector<std::pair<int, int>> selectedDemand(
            std::vector<DetailRequest> requests
        )
        {
            std::sort(
                requests.begin(),
                requests.end(),
                [](const auto& a, const auto& b)
                {
                    if (a.priority != b.priority)
                    {
                        return a.priority < b.priority;
                    }
                    return std::pair{a.y, a.x} < std::pair{b.y, b.x};
                }
            );
            std::vector<std::pair<int, int>> result;
            result.reserve(std::min(requests.size(), MaximumDetailChunks));
            for (std::size_t i = 0;
                 i < std::min(requests.size(), MaximumDetailChunks);
                 ++i)
            {
                result.emplace_back(requests[i].x, requests[i].y);
            }
            return result;
        }

        void advanceDetail(
            Renderer& renderer,
            const World& world,
            const std::vector<DetailRequest>& requests
        )
        {
            for (const auto request : requests)
            {
                const auto found = std::find_if(
                    currentDemand.begin(),
                    currentDemand.end(),
                    [&](const auto& r)
                    { return r.x == request.x && r.y == request.y; }
                );
                if (found == currentDemand.end())
                {
                    currentDemand.push_back(request);
                }
                else
                {
                    found->priority =
                        std::min(found->priority, request.priority);
                }
            }
            // Both projections contribute to one residency goal. The preceding
            // completed frame supplies tangent-only requests to the next sphere
            // pass, so they progress even when its raster work uses the budget.
            auto combined = previousDemand;
            for (const auto request : currentDemand)
            {
                const auto found = std::find_if(
                    combined.begin(),
                    combined.end(),
                    [&](const auto& r)
                    { return r.x == request.x && r.y == request.y; }
                );
                if (found == combined.end())
                {
                    combined.push_back(request);
                }
                else
                {
                    found->priority =
                        std::min(found->priority, request.priority);
                }
            }
            const auto wanted = selectedDemand(std::move(combined));
            const auto updatePending = [&]
            {
                detailDemandPending = std::any_of(
                    wanted.begin(),
                    wanted.end(),
                    [&](const auto& p)
                    { return !detail.contains(key(p.first, p.second)); }
                );
            };
            updatePending();
            // Never combine a page from a new influence revision with the
            // previous complete coarse presentation during a refresh.
            if (preparation || !ready)
            {
                return;
            }
            for (const auto [x, y] : wanted)
            {
                const auto it = detail.find(key(x, y));
                if (it != detail.end())
                {
                    it->second.used = frame;
                }
            }
            // Visibility scans and the other projection's drawing do not spend
            // this budget. Charge only actual cache work, so a wide viewport
            // cannot consume the deadline before the first sample row starts.
            WorkSlice slice(*this);
            while (timeRemaining())
            {
                if (!detailRaster)
                {
                    auto missing = std::find_if(
                        wanted.begin(),
                        wanted.end(),
                        [&](const auto& p)
                        { return !detail.contains(key(p.first, p.second)); }
                    );
                    if (missing == wanted.end())
                    {
                        return;
                    }
                    if (detail.size() >= MaximumDetailChunks)
                    {
                        auto oldest = std::min_element(
                            detail.begin(),
                            detail.end(),
                            [](const auto& a, const auto& b)
                            { return a.second.used < b.second.used; }
                        );
                        if (oldest->second.used == frame)
                        {
                            return;
                        }
                        detail.erase(oldest);
                    }
                    detailRaster = std::make_unique<PoliticalRaster>(
                        missing->first,
                        missing->second,
                        std::min(ChunkSide, width - missing->first),
                        std::min(ChunkSide, height - missing->second),
                        WorldPixelsPerTile, true
                    );
                }
                if (!advanceRaster(
                        renderer,
                        world,
                        *surfaceSource,
                        *presentedInfluence,
                        presentedPalette,
                        distance,
                        *detailRaster, presentedPopulation.get()
                    ))
                {
                    return;
                }
                const auto pageKey = key(detailRaster->x, detailRaster->y);
                detail.emplace(pageKey, std::move(detailRaster->result));
                detailRaster.reset();
                ++builds;
                updatePending();
            }
        }

        void draw(
            Renderer& r,
            const PoliticalTextures& t,
            const WorldPresentationState& p
        )
        {
            const auto layer = [&](const Texture* tex, float weight)
            {
                if (!tex || weight <= .001F || indices.empty())
                {
                    return;
                }
                const auto a = byte(255 * weight);
                for (auto& v : vertices)
                {
                    v.color = {255, 255, 255, a};
                }
                r.drawMesh(*tex, vertices, indices);
            };
            layer(t.fill.get(), p.realmFillWeight);
            layer(t.border.get(), p.realmBorderWeight);
            if (selected && t.selectedFor == selected) layer(t.selectedBorder.get(), 1.F);
        }

        void flatPage(
            Renderer& r,
            const PoliticalTextures& t,
            const Camera2D& c,
            double pixels,
            int x,
            int y,
            int w,
            int h,
            const WorldPresentationState& p
        )
        {
            const double pitch = r.currentPixelPitch();
            const auto snap = [pitch](double p)
            { return float(std::round(p / pitch) * pitch); };
            const double originX =
                r.outputWidth() * .5 + (x - c.tileX()) * pixels;
            const double originY =
                r.outputHeight() * .5 + (y - c.tileY()) * pixels;
            const float left = snap(originX), top = snap(originY);
            const float right = snap(originX + w * pixels),
                        bottom = snap(originY + h * pixels);
            const float u0 = float(double(x - t.x) / t.width),
                        u1 = float(double(x + w - t.x) / t.width);
            const float v0 = float(double(y - t.y) / t.height),
                        v1 = float(double(y + h - t.y) / t.height);
            vertices = {
                {left, top, u0, v0, {}},
                {right, top, u1, v0, {}},
                {right, bottom, u1, v1, {}},
                {left, bottom, u0, v1, {}}
            };
            indices = {0, 1, 2, 0, 2, 3};
            draw(r, t, p);
        }

        void spherePage(
            Renderer& r,
            const PoliticalTextures& t,
            const GlobeView& view,
            int x,
            int y,
            int w,
            int h,
            bool detailed,
            const WorldPresentationState& p
        )
        {
            vertices.clear();
            indices.clear();
            struct V
            {
                WorldSurface::Point3 position;
                double u, v;
            };
            const auto vertex = [&](double tx, double ty)
            {
                return V{
                    view.orient(WorldSurface::sphere(tx / width, ty / height)),
                    (tx - t.x) / t.width,
                    (ty - t.y) / t.height
                };
            };
            const auto triangle = [&](V a, V b, V c)
            {
                // The same horizon clipping as terrain, rather than dropping
                // crossing triangles and leaving a strip of unpainted
                // coastline.
                const std::array<V, 3> in{a, b, c};
                std::array<V, 5> poly{};
                int count = 0;
                for (int i = 0; i < 3; ++i)
                {
                    const auto& p = in[i];
                    const auto& q = in[(i + 1) % 3];
                    if (p.position.z >= 0)
                    {
                        poly[count++] = p;
                    }
                    if ((p.position.z >= 0) != (q.position.z >= 0))
                    {
                        const double f =
                            p.position.z / (p.position.z - q.position.z);
                        poly[count++] = {
                            {std::lerp(p.position.x, q.position.x, f),
                             std::lerp(p.position.y, q.position.y, f),
                             0},
                            std::lerp(p.u, q.u, f),
                            std::lerp(p.v, q.v, f)
                        };
                    }
                }
                if (count < 3)
                {
                    return;
                }
                const int first = int(vertices.size());
                for (int i = 0; i < count; ++i)
                {
                    vertices.push_back(
                        {float(view.cx + poly[i].position.x * view.radius),
                         float(view.cy - poly[i].position.y * view.radius),
                         float(poly[i].u),
                         float(poly[i].v),
                         {}}
                    );
                }
                for (int i = 1; i < count - 1; ++i)
                {
                    indices.insert(
                        indices.end(),
                        {first, first + i, first + i + 1}
                    );
                }
            };
            const int cols = detailed ? (w + 1) / 2 : 96;
            const int rows = detailed ? (h + 1) / 2 : 48;
            for (int j = 0; j < rows; ++j)
            {
                for (int i = 0; i < cols; ++i)
                {
                    const double x0 =
                        detailed ? x + i * 2. : x + double(w) * i / cols;
                    const double x1 = detailed ? x + std::min(w, (i + 1) * 2)
                                               : x + double(w) * (i + 1) / cols;
                    const double y0 =
                        detailed ? y + j * 2. : y + double(h) * j / rows;
                    const double y1 = detailed ? y + std::min(h, (j + 1) * 2)
                                               : y + double(h) * (j + 1) / rows;
                    auto a = vertex(x0, y0), b = vertex(x1, y0),
                         c = vertex(x1, y1), d = vertex(x0, y1);
                    triangle(a, b, c);
                    triangle(a, c, d);
                }
            }
            draw(r, t, p);
        }

        void drawLabels(
            Renderer& r,
            const World& world,
            const Camera2D& c,
            double pixels,
            bool globe,
            const WorldPresentationState& p,
            const WorldPresentationPolicy& policy
        )
        {
            if (p.realmLabelWeight <= .001F)
            {
                return;
            }
            const auto view = GlobeView::from(
                c,
                world.grid(),
                r.outputWidth(),
                r.outputHeight()
            );
            for (const auto& l : labels)
            {
                const auto* realm = world.realm(l.id);
                if (!realm || !l.count || realm->name().empty())
                {
                    continue;
                }
                auto at = globe
                              ? view.project(
                                    (l.anchor.x + .5) / width,
                                    (l.anchor.y + .5) / height
                                )
                              : WorldSurface::Point3{
                                    r.outputWidth() * .5 +
                                        (l.anchor.x + .5 - c.tileX()) * pixels,
                                    r.outputHeight() * .5 +
                                        (l.anchor.y + .5 - c.tileY()) * pixels,
                                    1
                                };
                if (at.z <= .15 || at.x < -300 || at.y < -50 ||
                    at.x > r.outputWidth() + 300 ||
                    at.y > r.outputHeight() + 50)
                {
                    continue;
                }
                // Existing realm-name sizing/contrast policy is unchanged.
                const float natural = font.measureWidth(realm->name(), 1);
                const float size = std::clamp(
                    float(
                        std::max(1., std::sqrt(double(l.count)) * 1.8) * pixels
                    ) * policy.realmLabelTerritoryWidthFraction /
                        std::max(1.F, natural),
                    std::max(.5F, policy.minimumRealmLabelPixelSize),
                    std::max(.5F, policy.maximumRealmLabelPixelSize)
                );
                const auto color = realm->mapColor();
                const bool dark = 299 * int(color.red) +
                                      587 * int(color.green) +
                                      114 * int(color.blue) >
                                  145000;
                RenderColor ink = dark ? RenderColor{24, 27, 32, 255}
                                       : RenderColor{244, 243, 232, 255};
                RenderColor shadow = dark ? RenderColor{255, 255, 255, 210}
                                          : RenderColor{8, 15, 27, 225};
                const double alpha =
                    p.realmLabelWeight * std::clamp(at.z * 3, 0., 1.);
                ink.alpha = byte(ink.alpha * alpha);
                shadow.alpha = byte(shadow.alpha * alpha);
                const float x = float(at.x) -
                                font.measureWidth(realm->name(), size) * .5F,
                            y = float(at.y) - 3.5F * size;
                font.drawText(r, realm->name(), x + 1, y + 1, size, shadow);
                font.drawText(r, realm->name(), x, y, size, ink);
            }
        }

        void prepareSelection(Renderer& renderer, PoliticalTextures& page)
        {
            if (!selected || page.selectedFor == selected) return;
            if (std::none_of(page.frontiers.begin(), page.frontiers.end(),
                [&](const auto& point) { return point.realm == selected; }))
            { page.selectedBorder.reset(); page.selectedFor = selected; return; }
            const int pw = page.width * page.density, ph = page.height * page.density;
            if (pw <= 0 || ph <= 0) return;
            std::vector<RenderColor> pixels(std::size_t(pw)*ph, {0,0,0,0});
            for (const auto& point : page.frontiers)
                if (point.realm == selected) pixels[std::size_t(point.y)*pw+point.x] = {235,196,107,255};
            if (!page.selectedBorder || page.selectedBorder->width()!=pw || page.selectedBorder->height()!=ph)
                page.selectedBorder = renderer.createTextureFromPixels(pw,ph,pixels);
            else if (!renderer.updateTexturePixels(*page.selectedBorder,pixels))
                throw std::runtime_error("Selection mask upload failed");
            if (!page.selectedBorder) throw std::runtime_error("Selection mask allocation failed");
            renderer.setTextureFiltering(*page.selectedBorder,false);
            page.selectedFor = selected;
        }

        void render(
            Renderer& r,
            const World& world,
            const Camera2D& c,
            double pixels,
            bool globe,
            const WorldPresentationState& p,
            const WorldPresentationPolicy& policy
        )
        {
            beginWorkFrame(r);
            if ((!thematicMapMode(mode) && world.realms().empty()) ||
                (!selected && p.realmFillWeight <= .001F && p.realmBorderWeight <= .001F &&
                 p.realmLabelWeight <= .001F))
            {
                detailRaster.reset();
                detailDemandPending = false;
                return;
            }
            {
                WorkSlice slice(*this);
                ensure(r, world);
                advancePreparation(r, world);
            }
            if (!ready || width <= 0 || height <= 0 || pixels <= 0 ||
                !std::isfinite(pixels))
            {
                return;
            }
            const auto view = GlobeView::from(
                c,
                world.grid(),
                r.outputWidth(),
                r.outputHeight()
            );
            if (pixels < 12)
            {
                prepareSelection(r, coarse);
                if (globe)
                {
                    spherePage(r, coarse, view, 0, 0, width, height, false, p);
                }
                else
                {
                    flatPage(r, coarse, c, pixels, 0, 0, width, height, p);
                }
            }
            else
            {
                struct Candidate
                {
                    int x, y;
                    double distance;
                };
                std::vector<Candidate> visible;
                for (int y = 0; y < height; y += ChunkSide)
                {
                    for (int x = 0; x < width; x += ChunkSide)
                    {
                        const int w = std::min(ChunkSide, width - x),
                                  h = std::min(ChunkSide, height - y);
                        double minX = 1e30, minY = 1e30, maxX = -1e30,
                               maxY = -1e30, maxZ = -1;
                        for (int j = 0; j <= 4; ++j)
                        {
                            for (int i = 0; i <= 4; ++i)
                            {
                                const double tx = x + w * i / 4.,
                                             ty = y + h * j / 4.;
                                const auto at =
                                    globe
                                        ? view.project(tx / width, ty / height)
                                        : WorldSurface::Point3{
                                              r.outputWidth() * .5 +
                                                  (tx - c.tileX()) * pixels,
                                              r.outputHeight() * .5 +
                                                  (ty - c.tileY()) * pixels,
                                              1
                                          };
                                minX = std::min(minX, at.x);
                                maxX = std::max(maxX, at.x);
                                minY = std::min(minY, at.y);
                                maxY = std::max(maxY, at.y);
                                maxZ = std::max(maxZ, at.z);
                            }
                        }
                        if (maxZ < 0 || maxX < 0 || maxY < 0 ||
                            minX > r.outputWidth() || minY > r.outputHeight())
                        {
                            continue;
                        }
                        const double dx = (minX + maxX) * .5 -
                                          r.outputWidth() * .5,
                                     dy = (minY + maxY) * .5 -
                                          r.outputHeight() * .5;
                        visible.push_back({x, y, dx * dx + dy * dy});
                    }
                }
                std::stable_sort(
                    visible.begin(),
                    visible.end(),
                    [](auto a, auto b) { return a.distance < b.distance; }
                );
                std::vector<DetailRequest> wanted;
                wanted.reserve(std::min(visible.size(), MaximumDetailChunks));
                for (std::size_t i = 0;
                     i < std::min(visible.size(), MaximumDetailChunks);
                     ++i)
                {
                    wanted.push_back(
                        {visible[i].x, visible[i].y, visible[i].distance}
                    );
                }
                // Prioritize a stable resident set when a large viewport sees
                // more pages than the existing 32 MiB cache can hold. Do not
                // continually evict one visible page to rebuild another.
                advanceDetail(r, world, wanted);
                for (const auto v : visible)
                {
                    const auto found = detail.find(key(v.x, v.y));
                    auto* page =
                        found == detail.end() ? &coarse : &found->second;
                    prepareSelection(r, *page);
                    const int w = std::min(ChunkSide, width - v.x),
                              h = std::min(ChunkSide, height - v.y);
                    // Exactly one contribution per surface patch. Painting an
                    // alpha detail page over a coarse page would double the
                    // tint.
                    if (globe)
                    {
                        spherePage(r, *page, view, v.x, v.y, w, h, true, p);
                    }
                    else
                    {
                        flatPage(r, *page, c, pixels, v.x, v.y, w, h, p);
                    }
                }
            }
            drawLabels(r, world, c, pixels, globe, p, policy);
        }
    };

    WorldRealmPresentationRenderer::WorldRealmPresentationRenderer()
        : cache_(std::make_unique<Cache>())
    {
    }
    WorldRealmPresentationRenderer::~WorldRealmPresentationRenderer() = default;
    void WorldRealmPresentationRenderer::reset()
    {
        cache_ = std::make_unique<Cache>();
    }
    void WorldRealmPresentationRenderer::configure(WorldMapMode mode, RealmId selected)
    {
        // Terrain shares the political cache so toggling P/T is immediate.
        const auto cacheMode=mode==WorldMapMode::Terrain?WorldMapMode::Political:mode;
        if (cache_->mode!=cacheMode)
        {
            cache_=std::make_unique<Cache>();
            cache_->mode=cacheMode;
        }
        cache_->selected=selected;
    }
    bool WorldRealmPresentationRenderer::prepare(Renderer& r, const World& w)
    {
        cache_->beginWorkFrame(r, !cache_->ready);
        {
            Cache::WorkSlice slice(*cache_);
            cache_->ensure(r, w);
            cache_->advancePreparation(r, w);
        }
        return cache_->ready;
    }
    bool WorldRealmPresentationRenderer::preparationReady() const noexcept
    {
        return cache_->ready;
    }
    bool WorldRealmPresentationRenderer::hasPendingWork() const noexcept
    {
        return bool(cache_->preparation) || bool(cache_->detailRaster) ||
               cache_->detailDemandPending;
    }
    void WorldRealmPresentationRenderer::renderFlat(
        Renderer& r,
        const World& w,
        const Camera2D& c,
        const TileRenderMetrics& m,
        const WorldPresentationState& p,
        const WorldPresentationPolicy& policy
    )
    {
        cache_->render(r, w, c, m.scaledTilePixels(c.zoom()), false, p, policy);
    }
    void WorldRealmPresentationRenderer::renderGlobe(
        Renderer& r,
        const World& w,
        const Camera2D& c,
        const WorldPresentationState& p,
        const WorldPresentationPolicy& policy
    )
    {
        const auto view =
            GlobeView::from(c, w.grid(), r.outputWidth(), r.outputHeight());
        cache_->render(
            r,
            w,
            c,
            view.radius * 2 * Pi / std::max(1, w.grid().width()),
            true,
            p,
            policy
        );
    }
    std::uint64_t WorldRealmPresentationRenderer::cacheBuilds() const noexcept
    {
        return cache_->builds;
    }
    std::size_t WorldRealmPresentationRenderer::
        detailCacheBytes() const noexcept
    {
        std::size_t n = 0;
        for (const auto& [key, t] : cache_->detail)
        {
            n += t.bytes;
            if (t.selectedBorder) n += std::size_t(t.selectedBorder->width()) * t.selectedBorder->height() * 4;
        }
        return n;
    }
} // namespace Paladin
