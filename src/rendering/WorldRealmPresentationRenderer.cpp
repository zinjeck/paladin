#include "rendering/WorldRealmPresentationRenderer.h"
#include "rendering/WorldPoliticalSurface.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldPixelGrid.h"
#include "ui/BitmapFontRenderer.h"

#include <algorithm>
#include <array>
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
        constexpr std::size_t MaximumDetailChunks = 64; // <= 32 MiB GPU
        constexpr int BuildsPerFrame = 4;
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


        struct PoliticalTextures
        {
            std::unique_ptr<Texture> fill, border;
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
    }

    struct WorldRealmPresentationRenderer::Cache
    {
        const World* source = nullptr;
        const Renderer* rendererOwner = nullptr;
        std::uint64_t signature = 0, builds = 0, frame = 0;
        int width = 0, height = 0;
        PoliticalTextures coarse;
        std::unordered_map<std::uint64_t, PoliticalTextures> detail;
        std::vector<float> distance;
        std::vector<Label> labels;
        std::vector<MeshVertex> vertices;
        std::vector<int> indices;
        BitmapFontRenderer font;

        static std::uint64_t key(int x, int y)
        { return (std::uint64_t(std::uint32_t(y)) << 32) | std::uint32_t(x); }

        void ensure(Renderer& renderer, const World& world)
        {
            const auto& influence = world.tribalInfluence();
            std::uint64_t next = 1469598103934665603ULL;
            const auto mix = [&](std::uint64_t value) { next = (next ^ value) * 1099511628211ULL; };
            mix(world.grid().revision()); mix(world.territory().revision());
            mix(influence.revision()); mix(world.grid().width()); mix(world.grid().height());
            for (const auto& r : world.realms())
            {
                mix(r.id().value()); mix(r.usesTribalInfluence());
                const auto c = r.mapColor(); mix(c.red); mix(c.green); mix(c.blue);
                for (unsigned char ch : r.name()) mix(ch);
            }
            if (source == &world && rendererOwner == &renderer && next == signature) return;
            source = &world; rendererOwner = &renderer; signature = next;
            width = world.grid().width(); height = world.grid().height();
            coarse = {}; detail.clear(); labels.clear(); distance.clear();
            if (width <= 0 || height <= 0) return;

            // One coarse signed-inside distance for restrained relief. The final
            // per-pixel owner mask, NOT this distance, clips fill and border.
            std::vector<RealmId> owners(std::size_t(width) * height);
            distance.assign(owners.size(), 5.F);
            const auto index = [&](int x, int y) { return std::size_t(y) * width + (x % width + width) % width; };
            const auto owner = [&](int x, int y) -> RealmId {
                return y < 0 || y >= height ? RealmId{} : owners[index(x,y)];
            };
            std::unordered_map<RealmId, std::size_t, StrongIdHash> byRealm;
            for (const auto& r : world.realms())
            {
                byRealm[r.id()] = labels.size(); labels.push_back({r.id()});
            }
            std::vector<RealmId> labelOwners(owners.size());
            const double threshold = world.territoryFoundationPolicy().tribalInfluence.visibleInfluenceThreshold;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            {
                const auto surface = worldPoliticalSurfaceAt(world, x + .5, y + .5);
                owners[index(x,y)] = surface.civic;
                RealmId id = surface.civic;
                if (surface.land && !id)
                {
                    const auto t = influence.sampleAt({x,y});
                    if (t.primaryInfluence > threshold) id = t.primaryRealm;
                }
                labelOwners[index(x,y)] = id;
                const auto it = byRealm.find(id);
                if (it == byRealm.end()) continue;
                auto& l = labels[it->second];
                const double longitude = (x + .5) / width * 2 * Pi;
                ++l.count; l.sine += std::sin(longitude); l.cosine += std::cos(longitude); l.sumY += y + .5;
            }
            std::queue<WorldTilePosition> queue;
            constexpr std::array<WorldTilePosition, 4> offsets{{{-1,0},{1,0},{0,-1},{0,1}}};
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            {
                const auto id = owner(x,y);
                if (!id) { distance[index(x,y)] = 0; continue; }
                for (auto o : offsets) if (owner(x+o.x,y+o.y) != id)
                { distance[index(x,y)] = .5F; queue.push({x,y}); break; }
            }
            while (!queue.empty())
            {
                const auto p = queue.front(); queue.pop();
                const float d = distance[index(p.x,p.y)] + 1;
                if (d >= 5) continue;
                for (auto o : offsets)
                {
                    const int x = (p.x+o.x+width)%width, y = p.y+o.y;
                    if (y < 0 || y >= height || owner(x,y) != owner(p.x,p.y)) continue;
                    if (distance[index(x,y)] > d) { distance[index(x,y)] = d; queue.push({x,y}); }
                }
            }
            for (auto& l : labels) if (l.count)
            {
                double angle = std::atan2(l.sine, l.cosine); if (angle < 0) angle += 2*Pi;
                l.x = angle / (2*Pi) * width; l.y = l.sumY / l.count;
            }
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            {
                const auto it = byRealm.find(labelOwners[index(x,y)]);
                if (it == byRealm.end()) continue;
                auto& l = labels[it->second];
                double dx = std::abs(x+.5-l.x); dx = std::min(dx, width-dx);
                const double d = dx*dx + (y+.5-l.y)*(y+.5-l.y);
                if (d < l.best) { l.best = d; l.anchor = {x,y}; }
            }
            const int density = std::clamp(2048 / std::max(width,height), 1, 4);
            coarse = raster(renderer, world, influence, 0, 0, width, height, density);
        }

        float relief(const WorldPoliticalSurfaceSample& s) const
        {
            double sum = 0;
            for (int i = 0; i < 4; ++i)
                sum += s.dryWeights[i] * distance[std::size_t(s.positions[i].y)*width+s.positions[i].x];
            return float(sum / std::max(.000001, s.landWeight));
        }

        PoliticalTextures raster(Renderer& renderer, const World& world,
                                  const TribalInfluenceMap& influence,
                                  int x, int y, int w, int h, int density)
        {
            const int pw = w*density, ph = h*density, stride = pw+2;
            // The one-sample halo uses absolute map coordinates. Neighboring
            // pages therefore produce identical contours, including page corners.
            std::vector<WorldPoliticalSurfaceSample> samples(std::size_t(stride)*(ph+2));
            for (int j = -1; j <= ph; ++j) for (int i = -1; i <= pw; ++i)
                samples[std::size_t(j+1)*stride+i+1] = worldPoliticalSurfaceAt(world, x+(i+.5)/density, y+(j+.5)/density);
            std::vector<RenderColor> fills(std::size_t(pw)*ph, {0,0,0,0});
            std::vector<RenderColor> borders(fills.size(), {0,0,0,0});
            bool hasFill = false, hasBorder = false;
            const auto& policy = world.territoryFoundationPolicy().tribalInfluence;
            for (int j = 0; j < ph; ++j) for (int i = 0; i < pw; ++i)
            {
                const auto n = std::size_t(j+1)*stride+i+1;
                const auto out = std::size_t(j)*pw+i;
                const auto& s = samples[n];
                if (!s.land) continue;
                if (const auto* realm = world.realm(s.civic))
                {
                    const auto c = realm->mapColor();
                    const bool edge = samples[n-1].civic != s.civic || samples[n+1].civic != s.civic ||
                                      samples[n-stride].civic != s.civic || samples[n+stride].civic != s.civic;
                    const double shade = .86 + .14 * smoothStep(0, 5, edge ? 0 : relief(s));
                    fills[out] = {byte(c.red*shade), byte(c.green*shade), byte(c.blue*shade), 255};
                    if (edge)
                    {
                        // Ink stays strictly on the dry, owned side of the SAME
                        // mask. There is no separately positioned outline mesh.
                        borders[out] = {byte(c.red*.52), byte(c.green*.52), byte(c.blue*.52), 255};
                        hasBorder = true;
                    }
                }
                else fills[out] = tribalPixel(world, worldTribalSurfaceSample(influence, s), policy);
                hasFill |= fills[out].alpha != 0;
            }
            PoliticalTextures t;
            t.x=x; t.y=y; t.width=w; t.height=h; t.used=frame;
            if (hasFill) t.fill = renderer.createTextureFromPixels(pw, ph, fills);
            if (hasBorder) t.border = renderer.createTextureFromPixels(pw, ph, borders);
            if ((hasFill && !t.fill) || (hasBorder && !t.border)) throw std::runtime_error("Political surface cache upload failed");
            // Nearest sampling retains exact coast cutouts. Both layers use the
            // same atlas resolution, UVs, geometry and projection at every zoom.
            if (t.fill) renderer.setTextureFiltering(*t.fill, false);
            if (t.border) renderer.setTextureFiltering(*t.border, false);
            t.bytes = std::size_t(pw)*ph*4*(bool(t.fill)+bool(t.border));
            ++builds;
            return t;
        }

        PoliticalTextures* detailFor(Renderer& r, const World& world, int x, int y, int& remaining)
        {
            auto it = detail.find(key(x,y));
            if (it != detail.end()) { it->second.used = frame; return &it->second; }
            if (remaining == 0) return nullptr;
            if (detail.size() >= MaximumDetailChunks)
            {
                auto oldest = std::min_element(detail.begin(), detail.end(), [](const auto& a, const auto& b) {
                    return a.second.used < b.second.used;
                });
                if (oldest->second.used == frame) return nullptr;
                detail.erase(oldest);
            }
            --remaining;
            auto t = raster(r, world, world.tribalInfluence(), x, y,
                            std::min(ChunkSide,width-x), std::min(ChunkSide,height-y), WorldPixelsPerTile);
            return &detail.emplace(key(x,y), std::move(t)).first->second;
        }

        void draw(Renderer& r, const PoliticalTextures& t, const WorldPresentationState& p)
        {
            const auto layer = [&](const Texture* tex, float weight) {
                if (!tex || weight <= .001F || indices.empty()) return;
                const auto a = byte(255*weight);
                for (auto& v : vertices) v.color = {255,255,255,a};
                r.drawMesh(*tex, vertices, indices);
            };
            layer(t.fill.get(), p.realmFillWeight);
            layer(t.border.get(), p.realmBorderWeight);
        }

        void flatPage(Renderer& r, const PoliticalTextures& t, const Camera2D& c,
                      double pixels, int x, int y, int w, int h, const WorldPresentationState& p)
        {
            const double pitch=r.currentPixelPitch();
            const auto snap=[pitch](double p) { return float(std::round(p/pitch)*pitch); };
            const double originX=r.outputWidth()*.5+(x-c.tileX())*pixels;
            const double originY=r.outputHeight()*.5+(y-c.tileY())*pixels;
            const float left=snap(originX), top=snap(originY);
            const float right=snap(originX+w*pixels), bottom=snap(originY+h*pixels);
            const float u0=float(double(x-t.x)/t.width), u1=float(double(x+w-t.x)/t.width);
            const float v0=float(double(y-t.y)/t.height), v1=float(double(y+h-t.y)/t.height);
            vertices = {{left,top,u0,v0,{}},{right,top,u1,v0,{}},{right,bottom,u1,v1,{}},{left,bottom,u0,v1,{}}};
            indices = {0,1,2,0,2,3};
            draw(r,t,p);
        }

        void spherePage(Renderer& r, const PoliticalTextures& t, const GlobeView& view,
                        int x, int y, int w, int h, bool detailed, const WorldPresentationState& p)
        {
            vertices.clear(); indices.clear();
            struct V { WorldSurface::Point3 position; double u,v; };
            const auto vertex = [&](double tx, double ty) {
                return V{view.orient(WorldSurface::sphere(tx/width, ty/height)),
                         (tx-t.x)/t.width, (ty-t.y)/t.height};
            };
            const auto triangle = [&](V a, V b, V c) {
                // The same horizon clipping as terrain, rather than dropping
                // crossing triangles and leaving a strip of unpainted coastline.
                const std::array<V,3> in{a,b,c}; std::array<V,5> poly{}; int count=0;
                for (int i=0;i<3;++i)
                {
                    const auto& p=in[i]; const auto& q=in[(i+1)%3];
                    if (p.position.z >= 0) poly[count++]=p;
                    if ((p.position.z >= 0) != (q.position.z >= 0))
                    {
                        const double f=p.position.z/(p.position.z-q.position.z);
                        poly[count++]={{std::lerp(p.position.x,q.position.x,f), std::lerp(p.position.y,q.position.y,f),0},
                                        std::lerp(p.u,q.u,f),std::lerp(p.v,q.v,f)};
                    }
                }
                if (count<3) return;
                const int first=int(vertices.size());
                for (int i=0;i<count;++i)
                    vertices.push_back({float(view.cx+poly[i].position.x*view.radius),float(view.cy-poly[i].position.y*view.radius),float(poly[i].u),float(poly[i].v),{}});
                for (int i=1;i<count-1;++i) indices.insert(indices.end(),{first,first+i,first+i+1});
            };
            const int cols=detailed ? (w+1)/2 : 96;
            const int rows=detailed ? (h+1)/2 : 48;
            for (int j=0;j<rows;++j) for (int i=0;i<cols;++i)
            {
                const double x0=detailed?x+i*2.:x+double(w)*i/cols;
                const double x1=detailed?x+std::min(w,(i+1)*2):x+double(w)*(i+1)/cols;
                const double y0=detailed?y+j*2.:y+double(h)*j/rows;
                const double y1=detailed?y+std::min(h,(j+1)*2):y+double(h)*(j+1)/rows;
                auto a=vertex(x0,y0), b=vertex(x1,y0), c=vertex(x1,y1), d=vertex(x0,y1);
                triangle(a,b,c); triangle(a,c,d);
            }
            draw(r,t,p);
        }

        void drawLabels(Renderer& r, const World& world, const Camera2D& c,
                        double pixels, bool globe, const WorldPresentationState& p,
                        const WorldPresentationPolicy& policy)
        {
            if (p.realmLabelWeight <= .001F) return;
            const auto view=GlobeView::from(c,world.grid(),r.outputWidth(),r.outputHeight());
            for (const auto& l : labels)
            {
                const auto* realm=world.realm(l.id); if (!realm || !l.count || realm->name().empty()) continue;
                auto at=globe?view.project((l.anchor.x+.5)/width,(l.anchor.y+.5)/height):WorldSurface::Point3{
                    r.outputWidth()*.5+(l.anchor.x+.5-c.tileX())*pixels,
                    r.outputHeight()*.5+(l.anchor.y+.5-c.tileY())*pixels,1};
                if (at.z <= .15 || at.x < -300 || at.y < -50 || at.x > r.outputWidth()+300 || at.y > r.outputHeight()+50) continue;
                // Existing realm-name sizing/contrast policy is unchanged.
                const float natural=font.measureWidth(realm->name(),1);
                const float size=std::clamp(float(std::max(1.,std::sqrt(double(l.count))*1.8)*pixels)*policy.realmLabelTerritoryWidthFraction/std::max(1.F,natural),
                                           std::max(.5F,policy.minimumRealmLabelPixelSize),std::max(.5F,policy.maximumRealmLabelPixelSize));
                const auto color=realm->mapColor(); const bool dark=299*int(color.red)+587*int(color.green)+114*int(color.blue)>145000;
                RenderColor ink=dark?RenderColor{24,27,32,255}:RenderColor{244,243,232,255};
                RenderColor shadow=dark?RenderColor{255,255,255,210}:RenderColor{8,15,27,225};
                const double alpha=p.realmLabelWeight*std::clamp(at.z*3,0.,1.);
                ink.alpha=byte(ink.alpha*alpha); shadow.alpha=byte(shadow.alpha*alpha);
                const float x=float(at.x)-font.measureWidth(realm->name(),size)*.5F,y=float(at.y)-3.5F*size;
                font.drawText(r,realm->name(),x+1,y+1,size,shadow); font.drawText(r,realm->name(),x,y,size,ink);
            }
        }

        void render(Renderer& r, const World& world, const Camera2D& c, double pixels,
                    bool globe, const WorldPresentationState& p, const WorldPresentationPolicy& policy)
        {
            if (world.realms().empty() ||
                (p.realmFillWeight <= .001F && p.realmBorderWeight <= .001F && p.realmLabelWeight <= .001F)) return;
            ensure(r,world);
            if (width<=0 || height<=0 || pixels<=0 || !std::isfinite(pixels)) return;
            ++frame;
            const auto view=GlobeView::from(c,world.grid(),r.outputWidth(),r.outputHeight());
            if (pixels < 12)
            {
                if (globe) spherePage(r,coarse,view,0,0,width,height,false,p);
                else flatPage(r,coarse,c,pixels,0,0,width,height,p);
            }
            else
            {
                struct Candidate { int x,y; double distance; };
                std::vector<Candidate> visible;
                for (int y=0;y<height;y+=ChunkSide) for (int x=0;x<width;x+=ChunkSide)
                {
                    const int w=std::min(ChunkSide,width-x),h=std::min(ChunkSide,height-y);
                    double minX=1e30,minY=1e30,maxX=-1e30,maxY=-1e30,maxZ=-1;
                    for (int j=0;j<=4;++j) for (int i=0;i<=4;++i)
                    {
                        const double tx=x+w*i/4., ty=y+h*j/4.;
                        const auto at=globe?view.project(tx/width,ty/height):WorldSurface::Point3{
                            r.outputWidth()*.5+(tx-c.tileX())*pixels,r.outputHeight()*.5+(ty-c.tileY())*pixels,1};
                        minX=std::min(minX,at.x);maxX=std::max(maxX,at.x);minY=std::min(minY,at.y);maxY=std::max(maxY,at.y);maxZ=std::max(maxZ,at.z);
                    }
                    if (maxZ<0 || maxX<0 || maxY<0 || minX>r.outputWidth() || minY>r.outputHeight()) continue;
                    const double dx=(minX+maxX)*.5-r.outputWidth()*.5,dy=(minY+maxY)*.5-r.outputHeight()*.5;
                    visible.push_back({x,y,dx*dx+dy*dy});
                }
                std::stable_sort(visible.begin(),visible.end(),[](auto a,auto b){return a.distance<b.distance;});
                int remaining=BuildsPerFrame;
                for (const auto v : visible)
                {
                    auto* page=detailFor(r,world,v.x,v.y,remaining);
                    if (!page) page=&coarse;
                    const int w=std::min(ChunkSide,width-v.x),h=std::min(ChunkSide,height-v.y);
                    // Exactly one contribution per surface patch. Painting an
                    // alpha detail page over a coarse page would double the tint.
                    if (globe) spherePage(r,*page,view,v.x,v.y,w,h,true,p);
                    else flatPage(r,*page,c,pixels,v.x,v.y,w,h,p);
                }
            }
            drawLabels(r,world,c,pixels,globe,p,policy);
        }
    };

    WorldRealmPresentationRenderer::WorldRealmPresentationRenderer() : cache_(std::make_unique<Cache>()) {}
    WorldRealmPresentationRenderer::~WorldRealmPresentationRenderer() = default;
    void WorldRealmPresentationRenderer::reset() { cache_=std::make_unique<Cache>(); }
    void WorldRealmPresentationRenderer::renderFlat(Renderer& r,const World& w,const Camera2D& c,
        const TileRenderMetrics& m,const WorldPresentationState& p,const WorldPresentationPolicy& policy)
    { cache_->render(r,w,c,m.scaledTilePixels(c.zoom()),false,p,policy); }
    void WorldRealmPresentationRenderer::renderGlobe(Renderer& r,const World& w,const Camera2D& c,
        const WorldPresentationState& p,const WorldPresentationPolicy& policy)
    { const auto view=GlobeView::from(c,w.grid(),r.outputWidth(),r.outputHeight());
      cache_->render(r,w,c,view.radius*2*Pi/std::max(1,w.grid().width()),true,p,policy); }
    std::uint64_t WorldRealmPresentationRenderer::cacheBuilds() const noexcept { return cache_->builds; }
    std::size_t WorldRealmPresentationRenderer::detailCacheBytes() const noexcept
    { std::size_t n=0; for (const auto& [key,t] : cache_->detail) n+=t.bytes; return n; }
} // namespace Paladin
