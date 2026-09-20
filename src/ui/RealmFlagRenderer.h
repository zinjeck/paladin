#pragma once

#include "rendering/Renderer.h"
#include "world/FoundingIdentity.h"

namespace Paladin
{
    inline void drawRealmFlag(
        Renderer& renderer,
        const RealmFlag& flag,
        float x,
        float y,
        float cellSize = 4
    )
    {
        if (!flag.isValid())
        {
            return;
        }
        const float width = float(flag.width) * cellSize;
        const float height = float(flag.height) * cellSize;
        renderer.fillRectangle(
            x - 5,
            y - 5,
            width + 10,
            height + 10,
            {8, 15, 27, 255}
        );
        renderer.fillRectangle(
            x - 3,
            y - 3,
            width + 6,
            height + 6,
            {189, 134, 76, 255}
        );
        renderer
            .fillRectangle(x - 2, y - 2, width + 3, 1, {255, 232, 173, 255});
        renderer
            .fillRectangle(x - 2, y - 2, 1, height + 3, {255, 232, 173, 255});
        for (std::size_t i = 0; i < flag.cells.size(); ++i)
        {
            const auto& cell = flag.cells[i];
            const auto c = cell.painted ? cell.color : MapColor{32, 44, 67};
            renderer.fillRectangle(
                x + float(i % flag.width) * cellSize,
                y + float(i / flag.width) * cellSize,
                cellSize,
                cellSize,
                {c.red, c.green, c.blue, 255}
            );
        }
    }
} // namespace Paladin
