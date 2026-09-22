#pragma once
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/TerrainMaterialField.h"
#include "world/SettlementGrid.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    // Sample the solid mass, not a mountain icon at each storage tile. This is
    // evaluated only while baking cached terrain pages, never on camera frames.
    class CityReliefSurface
    {
    public:
        CityReliefSurface() = default;
        CityReliefSurface(const SettlementGrid& grid, int x, int y) : x_(x), y_(y)
        {
            for (int j = 0; j < 3; ++j) for (int i = 0; i < 3; ++i)
            {
                const auto* tile = grid.tile({std::clamp(x+i-1,0,grid.width()-1),
                                              std::clamp(y+j-1,0,grid.height()-1)});
                const bool solid = tile->terrain == TerrainType::Mountain;
                rock_[j*3+i] = solid ? 1.0 : 0.0;
                high_[j*3+i] = solid && tile->relief != ReliefType::Hills ? 1.0 : 0.0;
                active_ |= solid;
            }
            buried_ = std::all_of(high_.begin(),high_.end(),[](double value) { return value == 1; });
        }

        bool active() const { return active_; }

        static RenderColor stone(double x, double y, double facing = 0)
        {
            // Long shelves and broad facets share world-space coordinates.
            // Grain never supplies the silhouette or introduces tile-sized peaks.
            const double facet = landscapeField(x*.18,y*.13,2711);
            const double seam = landscapeField(x*.55,y*3.0,733);
            const double light = facet + facing*.22;
            if (seam < .16) { return {48,69,93,255}; }
            if (light > .72) { return {113,109,112,255}; }
            if (light > .42) { return {89,102,121,255}; }
            return {57,70,88,255};
        }

        RenderColor paint(double x, double y, RenderColor ground) const
        {
            if (!active_) { return ground; }
            if (buried_) { return {8,15,27,255}; }
            // Bounded, faceted chipping moves the boundary by less than a
            // quarter tile. Tile centres retain their navigation/mining meaning;
            // the canonical scene grid, not the storage grid, resolves the edge.
            const double chip = rockSurfaceNoise(x*2.3,y*2.3);
            const double sampleX = x + chip*.20 + rockSurfaceNoise(x*7,y*7)*.035;
            const double sampleY = y + rockSurfaceNoise(x*2.3+39,y*2.3-17)*.20 +
                                       rockSurfaceNoise(x*7-83,y*7+51)*.035;
            const double sx = sampleX-x_+.5, sy = sampleY-y_+.5;
            const int ix = int(sx), iy = int(sy), cell = iy*3+ix;
            const double ux = sx-ix, vy = sy-iy;
            const double u = ux, v = vy;
            const auto sample = [&](const auto& values)
            {
                return surfaceField(sampleX, sampleY, [&](int atX, int atY)
                { return values[(atY-y_+1)*3 + atX-x_+1]; }, false);
            };
            const double rock = sample(rock_);
            if (rock < .5) { return ground; }
            const double high = sample(high_);
            const double shelf = rockSurfaceNoise(x*.73,y*.73);
            if (high > .83+shelf*.08) { return {8,15,27,255}; }
            const double dx = ((rock_[cell+1]-rock_[cell])*(1-v) +
                               (rock_[cell+4]-rock_[cell+3])*v);
            const double dy = ((rock_[cell+3]-rock_[cell])*(1-u) +
                               (rock_[cell+4]-rock_[cell+1])*u);
            const double facing = std::clamp(dx*.7 + dy*.6,-1.0,1.0);
            if (high > .62+shelf*.06) { return {32,44,67,255}; }
            if (rock < .66+shelf*.05)
            {
                return facing > .15 && chip > -.28 ? RenderColor{154,167,175,255}
                                     : RenderColor{48,69,93,255};
            }
            return stone(x,y,facing);
        }

    private:
        std::array<double,9> rock_{}, high_{};
        int x_ = 0, y_ = 0;
        bool active_ = false;
        bool buried_ = false;
    };
}
