#include "rendering/SettlementEnvironmentDetails.h"
namespace Paladin
{
    bool naturalRoad(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SettlementMap& map,
        const SceneSpriteLibrary& sprites,
        const CompletedSettlementObject& object,
        std::uint64_t id
    )
    {
        const auto* art = sprites.find("road.floor");
        if (!art)
        {
            return false;
        }
        const auto occupied = [&](int x, int y)
        {
            const auto* neighbor = map.objectState().completedObjectAt({x, y});
            return neighbor &&
                   (neighbor->objectTypeId == SettlementObjectTypes::Road ||
                    neighbor->objectTypeId == SettlementObjectTypes::House ||
                    neighbor->objectTypeId == SettlementObjectTypes::CityKeep ||
                    neighbor->objectTypeId == SettlementObjectTypes::Bakery);
        };
        const auto& f = object.footprint;
        // Direct rendering is also the cache-budget fallback. Clip iteration
        // itself, not just draw calls, for long player-dragged road footprints.
        const int firstX = std::max(
            f.topLeft.x - 1,
            int(std::floor(p.cameraX - p.screenWidth * .5 / p.tilePixels)) - 1
        );
        const int firstY = std::max(
            f.topLeft.y - 1,
            int(std::floor(p.cameraY - p.screenHeight * .5 / p.tilePixels)) - 1
        );
        const int lastX = std::min(
            f.topLeft.x + f.width,
            int(std::ceil(p.cameraX + p.screenWidth * .5 / p.tilePixels)) + 1
        );
        const int lastY = std::min(
            f.topLeft.y + f.height,
            int(std::ceil(p.cameraY + p.screenHeight * .5 / p.tilePixels)) + 1
        );
        for (int ty = firstY; ty <= lastY; ++ty)
        {
            for (int tx = firstX; tx <= lastX; ++tx)
            {
                if (!p.visible(
                        p.bounds({double(tx), double(ty), 0, 1, 1, 0, 0})
                    ))
                {
                    continue;
                }
                const auto* land = map.grid().tile({tx, ty});
                if (!land || land->terrain != TerrainType::Land)
                {
                    continue;
                }
                if (!f.contains({tx, ty}))
                {
                    if (map.objectState().completedObjectAt({tx, ty}))
                    {
                        continue;
                    }
                    // One neighboring road owns each outer blend patch. This
                    // also rounds the empty inside corner of an L junction.
                    auto owner = std::numeric_limits<std::uint64_t>::max();
                    for (const auto d :
                         {SettlementTilePosition{0, -1},
                          {1, 0},
                          {0, 1},
                          {-1, 0}})
                    {
                        const auto* other = map.objectState().completedObjectAt(
                            {tx + d.x, ty + d.y}
                        );
                        if (other &&
                            other->objectTypeId == SettlementObjectTypes::Road)
                        {
                            owner = std::min(owner, other->id.value());
                        }
                    }
                    if (owner != object.id.value())
                    {
                        continue;
                    }
                }
                const int n = p.tilePixels >= 12 ? 16 : 4;
                for (int row = 0; row < n; ++row)
                {
                    for (int col = 0; col < n;)
                    {
                        const auto opacity = [&](int c)
                        {
                            const double xx = tx + (c + .5) / n,
                                         yy = ty + (row + .5) / n;
                            return roadSurfaceOpacity(
                                surfaceField(xx, yy, occupied),
                                xx,
                                yy
                            );
                        };
                        const int alpha = opacity(col), first = col++;
                        while (col < n && opacity(col) == alpha)
                        {
                            ++col;
                        }
                        groundPatch(
                            q,
                            p,
                            sprites,
                            *art,
                            tx + double(first) / n,
                            ty + double(row) / n,
                            double(col - first) / n,
                            1. / n,
                            id,
                            alpha,
                            -2
                        );
                    }
                }
            }
        }
        return true;
    }
    void buildingGround(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const SettlementMap& map,
        const SettlementObjectFootprint& f,
        std::uint64_t id
    )
    {
        const auto* soil = sprites.find("road.floor");
        if (!soil)
        {
            return;
        }
        const auto* building = map.objectState().completedObjectAt(f.topLeft);
        const bool northEntry = building && building->footprint == f &&
            building->objectTypeId == SettlementObjectTypes::House &&
            building->door && building->door->y == f.topLeft.y;
        std::vector<bool> northRoad(std::size_t(f.width), false);
        if (northEntry)
        {
            for (int column = 0; column < f.width; ++column)
            {
                const auto* neighbor = map.objectState().completedObjectAt(
                    {f.topLeft.x + column, f.topLeft.y - 1});
                northRoad[std::size_t(column)] = neighbor &&
                    neighbor->objectTypeId == SettlementObjectTypes::Road;
            }
        }
        // Keep the authored wall/foundation nearly against neighboring roads.
        // The cached dirt skirt is intentionally compact; close-view stones,
        // weeds and timber ends add breakup without creating a fake front yard.
        const double cell = .125;
        const double pad = .25;
        const double left = std::max(
            double(f.topLeft.x) - pad,
            p.cameraX - p.screenWidth * .5 / p.tilePixels - cell
        );
        const double top = std::max(
            double(f.topLeft.y) - pad,
            p.cameraY - p.screenHeight * .5 / p.tilePixels - cell
        );
        for (double y = std::floor(top / cell) * cell;
             y < f.topLeft.y + f.height + pad &&
             y < p.cameraY + p.screenHeight * .5 / p.tilePixels + cell;
             y += cell)
        {
            for (double x = std::floor(left / cell) * cell;
                 x < f.topLeft.x + f.width + pad &&
                 x < p.cameraX + p.screenWidth * .5 / p.tilePixels + cell;
                 x += cell)
            {
                const auto* land = map.grid().tile(
                    {int(std::floor(x + cell * .5)),
                     int(std::floor(y + cell * .5))}
                );
                if (!land || land->terrain != TerrainType::Land)
                {
                    continue;
                }
                const double dx =
                    std::abs(x + cell * .5 - (f.topLeft.x + f.width * .5)) -
                    (f.width * .5 - .25);
                const double dy =
                    std::abs(y + cell * .5 - (f.topLeft.y + f.height * .5)) -
                    (f.height * .5 - .25);
                const double distance =
                    std::hypot(std::max(0., dx), std::max(0., dy)) +
                    std::min(std::max(dx, dy), 0.) - .25;
                const double grain = .028 * std::sin(x * 9 + y * 5) +
                                     .018 * std::sin(y * 17 - x * 3);
                int alpha = int(
                    88 * std::clamp((.20 - distance + grain) / .34, 0., 1.)
                );
                const int column = int(std::floor(x + cell * .5)) - f.topLeft.x;
                if (northEntry && column >= 0 && column < f.width &&
                    northRoad[std::size_t(column)] &&
                    y + cell * .5 < f.topLeft.y + 1)
                {
                    // The hipped roof exposes small corners of the north wall
                    // ring. Join their ground to the adjacent road using that
                    // road's identical world-anchored material. The roof and
                    // usable interior are unchanged; cutaway floors draw above.
                    alpha = 255;
                }
                groundPatch(q, p, sprites, *soil, x, y, cell, cell, id, alpha);
            }
        }
    }
} // namespace Paladin
