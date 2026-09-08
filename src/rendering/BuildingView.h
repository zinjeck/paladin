#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneDetail.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementDoor.h"
#include <cmath>

namespace Paladin
{
    // World-space entrance heading is independent of the camera. Additional
    // angle samples can be added here when the camera gains continuous yaw.
    inline double buildingHeading(
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door
    )
    {
        if (!door)
        {
            return 180;
        }
        if (door->y == f.topLeft.y)
        {
            return 0;
        }
        if (door->x == f.topLeft.x + f.width - 1)
        {
            return 90;
        }
        if (door->y == f.topLeft.y + f.height - 1)
        {
            return 180;
        }
        return 270;
    }
    inline int buildingView(
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        double cameraHeading = 0
    )
    {
        const auto angle =
            std::fmod(buildingHeading(f, door) - cameraHeading + 720., 360.);
        return int(std::floor((angle + 45) / 90)) % 4;
    }
    // Shared families are selected by the object catalog. Legacy directional
    // exports remain supported by tribalBuilding when a family is unavailable.
    inline bool modularBuilding(
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
                    x,
                    y,
                    y,
                    id,
                    0,
                    w,
                    h
                );
            }
            else
            {
                sprites.surface(
                    queue,
                    p,
                    style.floor,
                    {x, y, 0, w, h, 0, 0},
                    {},
                    y,
                    id,
                    0,
                    false
                );
            }
            queue.setLayerFrom(start, -2);
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
                style.height
            );
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
            const auto wallStrip = [&](double xx,
                                       double yy,
                                       double ww,
                                       double hh,
                                       double depth,
                                       int part)
            {
                sprites.placed(queue, p, face, xx, yy, depth, id, part, ww, hh);
            };
            wallStrip(x, y + style.thickness, w, style.height, y, 1);
            // Caps follow actual doorway openings; changing art never changes
            // navigation.
            for (int row = 0; row < f.height; ++row)
            {
                if (!door ||
                    *door !=
                        SettlementTilePosition{f.topLeft.x, f.topLeft.y + row})
                {
                    wallStrip(x, y + row + 1, style.thickness, 1, y + row, 2);
                }
                if (!door || *door != SettlementTilePosition{
                                          f.topLeft.x + f.width - 1,
                                          f.topLeft.y + row
                                      })
                {
                    wallStrip(
                        x + w - style.thickness,
                        y + row + 1,
                        style.thickness,
                        1,
                        y + row,
                        2
                    );
                }
            }
            for (int col = 0; col < f.width; ++col)
            {
                if (!door || *door != SettlementTilePosition{
                                          f.topLeft.x + col,
                                          f.topLeft.y + f.height - 1
                                      })
                {
                    wallStrip(x + col, y + h, 1, style.thickness, y + h, 12);
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
                // Rows sharing a choices count use the same draw, allowing
                // mutually exclusive decorations at one attachment slot.
                const auto seed = id ^ (id >> 3) ^ (id >> 17);
                if (seed % piece.choices != piece.choice)
                {
                    continue;
                }
                // Negative Y anchors a wall attachment to the front wall plane.
                const double yy = piece.y < 0 ? y + h + piece.y : y + piece.y;
                const auto first = queue.size();
                sprites.placed(
                    queue,
                    p,
                    piece.sprite,
                    x + piece.x,
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

    inline bool tribalBuilding(
        SceneDrawQueue& queue,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CityPresentation& policy,
        const std::string& type,
        const SettlementObjectFootprint& f,
        std::optional<SettlementTilePosition> door,
        std::uint64_t id,
        double doorOpen = 0
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
