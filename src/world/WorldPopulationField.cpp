#include "world/WorldPopulationField.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <unordered_set>
#include "world/generation/GenerationNoise.h"

namespace Paladin
{
    std::uint64_t WorldPopulationField::fingerprint(const World& world) noexcept
    {
        std::uint64_t hash=1469598103934665603ULL;
        const auto mix=[&](std::uint64_t value) { hash=(hash^value)*1099511628211ULL; };
        mix(world.grid().revision());
        for(const auto& city:world.settlements())
        {
            mix(city.id().value()); mix(city.position().x); mix(city.position().y); mix(city.population()); mix(city.isFortress());
            if(const auto* map=city.simulationState().localMap())
            { mix(map->instanceId()); mix(map->objectState().presentationVersion()); mix(city.simulationState().citizens().version()); }
        }
        return hash;
    }
    WorldPopulationField::WorldPopulationField(const World& world)
        :width_(world.grid().width()),height_(world.grid().height()),people_(world.grid().tileCount(),0)
    {
        if(width_<=0 || height_<=0) return;
        const auto index=[&](WorldTilePosition p){return std::size_t(p.y)*width_+p.x;};
        const auto dry=[&](WorldTilePosition p){const auto* t=world.grid().tile(p); return t && t->terrain==TerrainType::Land;};
        const auto deposit=[&](WorldTilePosition p,double count)
        { p.x=(p.x%width_+width_)%width_; if(dry(p)) people_[index(p)]+=float(count); };
        std::unordered_set<std::size_t> roadCells;
        for (const auto& road : world.worldRoads()) for (const auto p : road.points())
            if (world.grid().isValidPosition(p)) roadCells.insert(index(p));
        for(const auto& city:world.settlements())
        {
            if(!city.population()) continue;
            const auto centre=city.position();
            if(const auto* map=city.simulationState().localMap())
            {
                const auto& citizens=city.simulationState().citizens();
                const double residents=double(citizens.residentCount());
                if(residents>0)
                {
                    for(const auto& person:citizens.citizens())
                    {
                        if(person.health<=0 || person.militaryDeployed) continue;
                        // Residence is stable while someone walks to work. This
                        // prevents roads flashing with an unrelated live heatmap.
                        auto local=person.tilePosition;
                        for(const auto& object:map->objectState().completedObjects())
                            if(object.id==person.homeId || (!person.homeId && object.objectTypeId==SettlementObjectTypes::CityKeep))
                            { local={object.footprint.topLeft.x+object.footprint.width/2,object.footprint.topLeft.y+object.footprint.height/2}; break; }
                        WorldTilePosition tile{map->sourceRegionCenter().x-map->sourceRegionWidth()/2+
                            int(std::floor((local.x+.5)/map->localTilesPerWorldTile())),
                            map->sourceRegionCenter().y-map->sourceRegionHeight()/2+
                            int(std::floor((local.y+.5)/map->localTilesPerWorldTile()))};
                        tile.x=(tile.x%width_+width_)%width_;
                        if(!dry(tile)) tile=centre;
                        deposit(tile,double(city.population())/residents);
                    }
                    continue;
                }
            }
            // A compact connected footprint, never the realm's entire colored
            // territory. About 40 people per urban world tile, with a denser
            // centre and terrain-dependent, reproducible edges. Fortress zones
            // occupy at most one third the width and height of city zones.
            const int radius=city.isFortress()?1:4;
            const std::size_t desired=std::size_t(std::clamp(std::ceil(double(city.population())/40.),1.,double((radius*2+1)*(radius*2+1))));
            std::vector<WorldTilePosition> cells{centre};
            constexpr std::array<WorldTilePosition,4> offsets{{{1,0},{0,1},{-1,0},{0,-1}}};
            for(std::size_t cursor=0;cursor<cells.size() && cells.size()<desired;++cursor)
                for(int direction=0;direction<4 && cells.size()<desired;++direction)
                {
                    const auto d=offsets[(direction+city.id().value()%4)%4];
                    WorldTilePosition p{(cells[cursor].x+d.x+width_)%width_,cells[cursor].y+d.y};
                    const int dx=std::min(std::abs(p.x-centre.x),width_-std::abs(p.x-centre.x));
                    if(dx>radius || std::abs(p.y-centre.y)>radius || !dry(p) || std::find(cells.begin(),cells.end(),p)!=cells.end()) continue;
                    cells.push_back(p);
                }
            double sum=0; std::vector<double> weights;
            for(const auto p:cells)
            {
                const int dx=std::min(std::abs(p.x-centre.x),width_-std::abs(p.x-centre.x)),dy=p.y-centre.y;
                const auto* terrain=world.grid().tile(p);
                const auto noise=GenerationNoise::mix(city.id().value()*7307ULL+index(p)*9176ULL);
                const double texture=.65+double(noise%1000)/1000.;
                const double access=roadCells.contains(index(p))?1.6:1.;
                const double slope=terrain?1.+2.*std::abs(double(terrain->elevation.value())-.5):1.;
                // Census is conserved below: this is a cartographic estimate of
                // where aggregate residents live, not new simulated population.
                const double weight=texture*access/(slope*(1.+.5*(dx*dx+dy*dy)));
                weights.push_back(weight); sum+=weight;
            }
            for(std::size_t i=0;i<cells.size();++i) deposit(cells[i],double(city.population())*weights[i]/sum);
        }
    }
    float WorldPopulationField::at(WorldTilePosition p) const noexcept
    { return width_<=0 || p.y<0 || p.y>=height_?0:people_[std::size_t(p.y)*width_+(p.x%width_+width_)%width_]; }
    double WorldPopulationField::total() const noexcept { return std::accumulate(people_.begin(),people_.end(),0.); }
}
