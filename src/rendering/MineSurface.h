#pragma once
#include "rendering/FramedWall.h"
#include "rendering/OutdoorGround.h"
#include "world/settlements/objects/jobs/mining/MiningJob.h"

namespace Paladin
{
    inline double mineServiceDepth(const SettlementObjectFootprint& f)
    {
        return miningServiceRows(f.height);
    }

    inline void mineSurface(SceneDrawQueue& queue, const SceneProjection& view,
        const SceneSpriteLibrary& sprites, const CompletedSettlementObject& object,
        std::uint64_t id, double workMinutes)
    {
        const auto& f = object.footprint;
        const double x = f.topLeft.x, y = f.topLeft.y;
        const double band = mineServiceDepth(f);
        const auto ground = queue.size();
        // Keep the approved earth material only on the service terrace.
        outdoorGround(queue, view, sprites, "market.floor",
            {x, y, 0, double(f.width), std::min(double(f.height), band + .5), 0, 0},
            {167, 141, 114, 255}, y, id);
        queue.setLayerFrom(ground, -3);

        // Old small saves keep a compact compound; newly drawn sites reserve
        // enough space for a real facade, stores and an unobstructed work lane.
        const double scale = std::min({1.0, f.width / 7.0, band / 4.0});
        const double pixel = 1.0 / 16;
        const double hw = 2.5 * scale;
        const double hx = x + f.width - hw - .375 * scale;
        const double roofTop = y + .5 * scale;
        const double base = y + 3.25 * scale;
        const double wallHeight = 1.0 * scale;
        const double roofHeight = 1.8125 * scale;
        const auto part = [&](double px, double py, double w, double h,
                              RenderColor color, double z, int order)
        {
            const auto b = view.bounds({px, py, 0, w, h, 0, 0});
            if (view.visible(b)) { queue.submit({b, color, z, id, 0, order}); }
        };
        // Ground contact stays under the walls. A separate raised roof leaves
        // a full plaster facade rather than painting a doorway over thatch.
        part(hx - .125 * scale, base - .125 * scale, hw + .3 * scale,
             .3125 * scale, {57,43,60,105}, base, 1);
        part(hx, base - wallHeight, hw, wallHeight,
             {167,141,114,255}, base, 11);
        framedWall(queue, view, sprites, "wall.adobe.front", hx, base,
                   hw, 0, wallHeight, base, id, 12, true);
        if (const auto* roof = sprites.find("roof.thatch.full"))
        {
            sprites.placed(queue, view, "roof.thatch.full",
                hx - .1875 * scale, roofTop + roof->elevation, base,
                id, 10, hw + .375 * scale, roofHeight);
        }
        part(hx, base - wallHeight, hw, .125 * scale,
             {57,43,60,115}, base, 21);
        // Door is safely smaller than wall and roof, with a visible lintel.
        sprites.placed(queue, view, "wall.adobe.door",
            hx + .65 * scale, base, base, id, 23, .4375 * scale, .6875 * scale);
        const double wx = hx + 1.65 * scale, wy = base - .65 * scale;
        part(wx - pixel, wy - pixel, .5625 * scale, .5 * scale,
             {73,53,47,255}, base, 24);
        part(wx, wy, .4375 * scale, .375 * scale,
             {32,44,67,255}, base, 25);
        part(wx + .1875 * scale, wy, pixel, .375 * scale,
             {183,131,80,255}, base, 26);
        part(wx, wy + .1875 * scale, .4375 * scale, pixel,
             {183,131,80,255}, base, 26);
        part(hx + 1.875 * scale, roofTop - .125 * scale,
             .3125 * scale, .5 * scale, {89,102,121,255}, base, 27);
        part(hx + 1.8125 * scale, roofTop - .1875 * scale,
             .4375 * scale, .125 * scale, {154,167,175,255}, base, 28);

        // Anchors are ground contacts (the catalog uses bottom-center pivots).
        const double hoistX = x + 1.0 * scale, hoistY = y + 2.0 * scale;
        part(hoistX - .625 * scale, hoistY - .1 * scale,
             1.375 * scale, .3125 * scale, {57,43,60,90}, hoistY, -2);
        sprites.placed(queue, view, "mine.hoist", hoistX, hoistY, hoistY,
                       id, 0, 1.375 * scale, 1.375 * scale);
        // A simulation-driven windlass rope and tub move only when a miner
        // actually works. Pausing or leaving the mine idle freezes the phase.
        const double motion = workMinutes >= 0 && view.tilePixels >= AnimationDetailPixels
            ? (1.0 + std::sin(workMinutes * 2.5)) * .1875 * scale : .1875 * scale;
        const double ropeX = hoistX + .0625 * scale;
        const double ropeY = hoistY - .875 * scale;
        part(ropeX, ropeY, pixel, .4375 * scale + motion,
             {235,196,107,255}, hoistY, 4);
        part(ropeX - .1875 * scale, ropeY + .375 * scale + motion,
             .4375 * scale, .3125 * scale, {73,53,47,255}, hoistY, 5);
        part(ropeX - .125 * scale, ropeY + .4375 * scale + motion,
             .3125 * scale, .125 * scale, {154,167,175,255}, hoistY, 6);

        // Two stores share a timber loading rack, outside the hut doorway.
        const double rackX = x + 2.0625 * scale, rackY = y + 1.625 * scale;
        part(rackX, rackY - .0625 * scale, 1.5 * scale, .1875 * scale,
             {57,43,60,100}, rackY, -2);
        part(rackX, rackY - .125 * scale, 1.375 * scale, .125 * scale,
             {116,81,63,255}, rackY, -1);
        for (int i = 0; i < 2; ++i)
        {
            sprites.placed(queue, view, "stockpile.crate",
                rackX + (.3125 + i * .75) * scale, rackY - .125 * scale,
                rackY, id, 5 + i, .5625 * scale, .4375 * scale);
        }
        // Wheelbarrow stands on its wheels beside the rack. Neither it nor
        // the hut decorates the mine floor or blocks the hoist approach.
        const double cartX = x + 2.25 * scale, cartY = y + 2.6875 * scale;
        part(cartX - .0625 * scale, cartY + .25 * scale,
             1.1875 * scale, .25 * scale, {57,43,60,95}, cartY + .5, 0);
        part(cartX, cartY, .875 * scale, .4375 * scale,
             {73,53,47,255}, cartY + .5, 1);
        part(cartX + pixel, cartY + pixel, .75 * scale, .1875 * scale,
             {183,131,80,255}, cartY + .5, 2);
        part(cartX + .125 * scale, cartY - .0625 * scale, .625 * scale, .25 * scale,
             {108,116,122,255}, cartY + .5, 3);
        part(cartX + .0625 * scale, cartY + .3125 * scale, .1875 * scale, .25 * scale,
             {8,15,27,255}, cartY + .5, 4);
        part(cartX + .625 * scale, cartY + .3125 * scale, .1875 * scale, .25 * scale,
             {8,15,27,255}, cartY + .5, 4);
        part(cartX + .8125 * scale, cartY + .125 * scale, .4375 * scale, pixel,
             {193,139,90,255}, cartY + .5, 5);
        part(cartX + .8125 * scale, cartY + .375 * scale, .4375 * scale, pixel,
             {193,139,90,255}, cartY + .5, 5);
    }
}
