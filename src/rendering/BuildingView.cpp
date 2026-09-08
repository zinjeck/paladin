#include "rendering/BuildingView.h"
namespace Paladin
{
    bool modularBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen
    )
    {
        const auto& style = sprites.objectStyle(type);
        const auto face = style.wall + ".front";
        if (!sprites.find(face))
        {
            return false;
        }
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        const auto interior = buildingInterior(f, type);
        const double ix = interior.topLeft.x, iy = interior.topLeft.y,
                     iw = interior.width, ih = interior.height;
        const double wallBand = (w - iw) * .5;
        const int facing = buildingView(f, door, policy.viewAzimuthDegrees);
        const bool side = h > w * 1.4 || (std::abs(w - h) < .5 && facing % 2);
        const auto roof = style.roof + (side ? ".full.side" : ".full");
        const bool roofVisible = policy.roofsVisible && sprites.find(roof);
        const auto start = queue.size();
        if (!roofVisible)
        {
            // Floor before furniture; rear wall and low caps cannot conceal
            // beds.
            if (sprites.find(style.floor + ".full"))
            {
                sprites.placed(
                    queue,
                    p,
                    style.floor + ".full",
                    ix,
                    iy,
                    y,
                    id,
                    0,
                    iw,
                    ih
                );
            }
            else
            {
                sprites.surface(
                    queue,
                    p,
                    style.floor,
                    {ix, iy, 0, iw, ih, 0, 0},
                    {},
                    y,
                    id,
                    0,
                    false
                );
            }
            if (door)
            {
                sprites.surface(
                    queue,
                    p,
                    style.floor,
                    {double(door->x), double(door->y), 0, 1, 1, 0, 0},
                    {},
                    y,
                    id,
                    0,
                    false
                );
            }
            queue.setLayerFrom(start, -2);
            if (policy.shadowsVisible)
            {
                // Non-overlapping penumbra bands stay inside the room; these
                // lie above the floor and below furniture and occupants.
                const double reach = std::min(ih, .25 * 1.1);
                for (int band = 0; band < 4; ++band)
                {
                    const double step = reach / 4;
                    queue.submit(
                        {p.bounds({ix, iy + band * step, 0, iw, step, 0, 0}),
                         {57, 43, 60, std::uint8_t(68 - band * 14)},
                         y - .01,
                         id,
                         -1,
                         0}
                    );
                }
                queue.submit(
                    {p.bounds(
                         {ix,
                          iy + reach,
                          0,
                          .125,
                          std::max(0., ih - reach),
                          0,
                          0}
                     ),
                     {57, 43, 60, 42},
                     y - .01,
                     id,
                     -1,
                     0}
                );
                queue.submit(
                    {p.bounds(
                         {ix + iw - .125,
                          iy + reach,
                          0,
                          .125,
                          std::max(0., ih - reach),
                          0,
                          0}
                     ),
                     {57, 43, 60, 28},
                     y - .01,
                     id,
                     -1,
                     0}
                );
            }
        }
        if (roofVisible)
        {
            if (const auto* art = sprites.find(roof))
            {
                sprites.placed(
                    queue,
                    p,
                    roof,
                    x - .22,
                    y + art->elevation - style.height,
                    y + h,
                    id,
                    10,
                    w + .44,
                    h + .08
                );
            }
            framedWall(
                queue,
                p,
                sprites,
                face,
                x,
                y + h,
                w,
                0,
                style.height,
                y + h,
                id,
                12,
                true
            );
            if (policy.shadowsVisible)
            {
                // The thatch overhang shades the top of the plaster face.
                for (int band = 0; band < 2; ++band)
                {
                    queue.submit(
                        {p.bounds(
                             {x,
                              y + h - style.height + band / 16.,
                              0,
                              w,
                              1. / 16.,
                              0,
                              0}
                         ),
                         {57, 43, 60, std::uint8_t(74 - band * 28)},
                         y + h,
                         id,
                         0,
                         13}
                    );
                }
            }
            // Door is an attachment, not baked into a work object's wall.
            if (door && facing == 2)
            {
                sprites.placed(
                    queue,
                    p,
                    style.wall + ".door",
                    door->x + .5,
                    y + h,
                    y + h + .01,
                    id,
                    14,
                    .55,
                    style.height * .88,
                    doorOpen
                );
            }
            if (door && facing % 2)
            {
                sprites.placed(
                    queue,
                    p,
                    style.wall + ".door",
                    facing == 1 ? x + w - .08 : x + .08,
                    door->y + 1,
                    y + h + .01,
                    id,
                    14,
                    .25,
                    style.height * .88,
                    doorOpen
                );
            }
        }
        else
        {
            // Keep the back wall tall; section the sides down and retain
            // only the front sill. Openings show floor, never a door leaf.
            const auto band = [&](double xx,
                                  double yy,
                                  double ww,
                                  double dd,
                                  double height,
                                  double depth,
                                  int part,
                                  bool horizontal)
            {
                const bool opening = door && door->x >= xx &&
                                     door->x < xx + ww && door->y >= yy &&
                                     door->y < yy + dd;
                const auto segment =
                    [&](double sx, double sy, double sw, double sd)
                {
                    if (sw <= 0 || sd <= 0)
                    {
                        return;
                    }
                    framedWall(
                        queue,
                        p,
                        sprites,
                        face,
                        sx,
                        sy,
                        sw,
                        sd,
                        height,
                        depth,
                        id,
                        part,
                        horizontal,
                        height < style.height
                    );
                };
                if (!opening)
                {
                    segment(xx, yy, ww, dd);
                }
                else if (horizontal)
                {
                    segment(xx, yy, door->x - xx, dd);
                    segment(door->x + 1, yy, xx + ww - door->x - 1, dd);
                }
                else
                {
                    segment(xx, yy, ww, door->y - yy);
                    segment(xx, door->y + 1, ww, yy + dd - door->y - 1);
                }
            };
            band(x, y, w, wallBand, .25, y, 2, true);
            band(
                x,
                y + wallBand,
                wallBand,
                h - 2 * wallBand,
                .25,
                y + h,
                2,
                false
            );
            band(
                x + w - wallBand,
                y + wallBand,
                wallBand,
                h - 2 * wallBand,
                .25,
                y + h,
                2,
                false
            );
            band(x, y + h - wallBand, w, wallBand, .25, y + h, 12, true);
            // One cap surface for the connected low ring, with boundary shading
            // only at its actual outline. Front corners have no join/divider.
            constexpr double cutHeight = .25, pixel = 1. / 16.;
            const auto* wallArt = sprites.find(face);
            const RenderColor base = wallArt->materialBase.alpha
                                         ? wallArt->materialBase
                                         : RenderColor{167, 141, 114, 255};
            const int width = f.width * 16, height = f.height * 16;
            const int border = int(wallBand * 16);
            const auto occupied = [&](int px, int py)
            {
                if (px < 0 || px >= width || py < 0 || py >= height)
                {
                    return false;
                }
                if (door && px / 16 == door->x - f.topLeft.x &&
                    py / 16 == door->y - f.topLeft.y)
                {
                    return false;
                }
                return px < border || px >= width - border || py < border ||
                       py >= height - border;
            };
            const auto capColor = [&](int px, int py)
            {
                if (!occupied(px, py))
                {
                    return RenderColor{0, 0, 0, 0};
                }
                if (!occupied(px - 1, py) || !occupied(px, py - 1))
                {
                    return RenderColor{169, 148, 120, 255};
                }
                if (!occupied(px + 1, py) || !occupied(px, py + 1))
                {
                    return RenderColor{116, 81, 63, 255};
                }
                const unsigned seed = unsigned(px + int(x * 16)) * 374761393u ^
                                      unsigned(py + int(y * 16)) * 668265263u;
                if ((seed ^ (seed >> 13)) % 31 == 0)
                {
                    return RenderColor{136, 96, 68, 255};
                }
                return base;
            };
            const auto colorAt = [&](int px, int py)
            {
                auto c = capColor(px, py);
                if (!c.alpha || !policy.shadowsVisible)
                {
                    return c;
                }
                // Equal-height caps have no taller wall casting onto them.
                // Their outline shading above preserves depth at every corner.
                const double shade = .035;
                c.red = std::uint8_t(c.red * (1 - shade) + 57 * shade);
                c.green = std::uint8_t(c.green * (1 - shade) + 43 * shade);
                c.blue = std::uint8_t(c.blue * (1 - shade) + 60 * shade);
                return c;
            };
            const int firstRow = std::max(
                0,
                int((p.cameraY - p.screenHeight * .5 / p.tilePixels - y +
                     cutHeight) *
                    16)
            );
            const int lastRow = std::min(
                height,
                int((p.cameraY + p.screenHeight * .5 / p.tilePixels - y +
                     cutHeight) *
                    16) +
                    1
            );
            const int left = std::max(
                0,
                int((p.cameraX - p.screenWidth * .5 / p.tilePixels - x) * 16)
            );
            const int right = std::min(
                width,
                int((p.cameraX + p.screenWidth * .5 / p.tilePixels - x) * 16) +
                    1
            );
            for (int py = firstRow; py < lastRow; ++py)
            {
                for (int px = left; px < right;)
                {
                    const auto color = colorAt(px, py);
                    int end = px + 1;
                    while (end < right)
                    {
                        const auto next = colorAt(end, py);
                        if (color.red != next.red ||
                            color.green != next.green ||
                            color.blue != next.blue ||
                            color.alpha != next.alpha)
                        {
                            break;
                        }
                        ++end;
                    }
                    if (color.alpha)
                    {
                        queue.submit(
                            {p.bounds(
                                 {x + px * pixel,
                                  y + py * pixel - cutHeight,
                                  0,
                                  (end - px) * pixel,
                                  pixel,
                                  0,
                                  0}
                             ),
                             color,
                             y + h,
                             id,
                             0,
                             20}
                        );
                    }
                    px = end;
                }
            }
        }
        // Shared decor pools and building-specific pieces compose
        // independently. A missing piece simply leaves that spot empty; no
        // inferred substitute.
        if (p.tilePixels >= StaticDetailPixels)
        {
            unsigned index = 0;
            for (const auto& piece : sprites.pieces())
            {
                ++index;
                if (piece.object != type && piece.object != style.decor)
                {
                    continue;
                }
                if (piece.state != "always" &&
                    piece.state != (roofVisible ? "roofed" : "cutaway"))
                {
                    continue;
                }
                if (!roofVisible && piece.y < 0)
                {
                    continue;
                }
                // Rows sharing a choices count use the same draw, allowing
                // mutually exclusive decorations at one attachment slot.
                const auto seed = id ^ (id >> 3) ^ (id >> 17);
                if (seed % piece.choices != piece.choice)
                {
                    continue;
                }
                // Negative Y anchors a wall attachment to the front wall plane.
                const double yy =
                    piece.y < 0 ? y + h + piece.y
                                : iy + std::min(piece.y, std::max(.5, ih - .4));
                const double px =
                    piece.y < 0
                        ? x + w * piece.x / std::max(1., style.moduleWidth)
                        : ix + std::min(piece.x, std::max(.5, iw - .4));
                const auto first = queue.size();
                sprites.placed(
                    queue,
                    p,
                    piece.sprite,
                    px,
                    yy,
                    piece.y < 0 ? y + h + .02 : yy + piece.depth,
                    id,
                    30 + index
                );
                if (!roofVisible)
                {
                    queue.setLayerFrom(first, -1);
                }
            }
        }
        return true;
    }
    bool tribalBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen
    )
    {
        if (modularBuilding(
                queue,
                p,
                sprites,
                policy,
                type,
                f,
                door,
                id,
                doorOpen
            ))
        {
            return true;
        }
        if (!policy.roofsVisible || !sprites.find(type + ".wall.front"))
        {
            return false;
        }
        const auto& style = sprites.objectStyle(type);
        const int facing = buildingView(f, door, policy.viewAzimuthDegrees);
        const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                     h = f.height;
        const auto roofName =
            type + ((h > w * 1.4 || (std::abs(w - h) < .5 && facing % 2))
                        ? ".roof.full.side"
                        : ".roof.full");
        const auto* roof = sprites.find(roofName);
        if (!roof)
        {
            return false;
        }
        // One fitted roof surface for the entire blueprint, including
        // extensions.
        sprites.placed(
            queue,
            p,
            roofName,
            x - .22,
            y,
            y + h,
            id,
            10,
            w + .44,
            h + .08
        );
        const auto face = type + (facing == 2   ? ".wall.front"
                                  : facing == 0 ? ".wall.back"
                                                : ".wall.side");
        sprites.placed(
            queue,
            p,
            face,
            x,
            y + h,
            y + h,
            id,
            12,
            w,
            style.height,
            facing == 2 ? doorOpen : 0
        );
        if (facing % 2)
        {
            // The entrance appears on its actual lateral edge. Its narrow
            // projected face also distinguishes the two side views.
            const double yy = door ? door->y + 1 : y + h * .65;
            const double xx = facing == 1 ? x + w - .12 : x - .28;
            sprites.placed(
                queue,
                p,
                type + ".wall.front",
                xx,
                yy,
                y + h + .01,
                id,
                13,
                .4,
                style.height,
                doorOpen
            );
        }
        return true;
    }
} // namespace Paladin
