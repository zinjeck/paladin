#pragma once

#include "world/SettlementTilePosition.h"
#include "world/TileGrid.h"
#include <cmath>

namespace Paladin
{
    enum class CityTileType : std::uint8_t
    {
        Inland,
        Beach,
        Coast,
        ShallowWater,
        DeepWater
    };
    class SettlementGrid final : public TileGrid<SettlementTilePosition>
    {
    public:
        using TileGrid<SettlementTilePosition>::TileGrid;
        CityTileType cityTileType(SettlementTilePosition p) const noexcept
        {
            if (!isValidPosition(p) || surfaces_.empty())
            {
                return CityTileType::Inland;
            }
            return surfaces_[std::size_t(p.y) * width() + p.x];
        }
        // City surfaces preserve walkable land and strategic-world terrain.
        void classifyCoast(std::uint64_t seed)
        {
            surfaces_.assign(tileCount(), CityTileType::Inland);
            std::vector<std::uint8_t> distance(tileCount(), 255);
            std::vector<SettlementTilePosition> frontier;
            constexpr SettlementTilePosition
                directions[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int y = 0; y < height(); ++y)
            {
                for (int x = 0; x < width(); ++x)
                {
                    const auto& t = *tile({x, y});
                    if (t.terrain != TerrainType::Water)
                    {
                        distance[std::size_t(y) * width() + x] = 0;
                        frontier.push_back({x, y});
                    }
                    else
                    {
                        surfaces_[std::size_t(y) * width() + x] =
                            CityTileType::DeepWater;
                    }
                }
            }
            for (std::size_t i = 0; i < frontier.size(); ++i)
            {
                const auto p = frontier[i];
                const auto d = distance[std::size_t(p.y) * width() + p.x];
                if (d >= 3)
                {
                    continue;
                }
                for (const auto n : directions)
                {
                    const SettlementTilePosition q{p.x + n.x, p.y + n.y};
                    if (!isValidPosition(q))
                    {
                        continue;
                    }
                    auto& next = distance[std::size_t(q.y) * width() + q.x];
                    if (next > d + 1)
                    {
                        next = std::uint8_t(d + 1);
                        frontier.push_back(q);
                        surfaces_[std::size_t(q.y) * width() + q.x] =
                            CityTileType::ShallowWater;
                    }
                }
            }
            for (int y = 0; y < height(); ++y)
            {
                for (int x = 0; x < width(); ++x)
                {
                    const auto& t = *tile({x, y});
                    if (t.terrain == TerrainType::Water)
                    {
                        continue;
                    }
                    bool touchesWater = false,
                         rocky = t.terrain == TerrainType::Mountain;
                    double slope = 0;
                    for (const auto n : directions)
                    {
                        if (const auto* neighbor = tile({x + n.x, y + n.y}))
                        {
                            touchesWater |=
                                neighbor->terrain == TerrainType::Water;
                            rocky |= neighbor->terrain == TerrainType::Mountain;
                            if (neighbor->terrain != TerrainType::Water)
                            {
                                slope = std::max(
                                    slope,
                                    std::abs(
                                        double(
                                            neighbor->elevation.value() -
                                            t.elevation.value()
                                        )
                                    )
                                );
                            }
                        }
                    }
                    if (!touchesWater)
                    {
                        continue;
                    }
                    const std::uint64_t patch =
                        (std::uint64_t(x / 8) * 73856093ULL) ^
                        (std::uint64_t(y / 8) * 19349663ULL) ^ seed;
                    const bool beach =
                        !rocky && slope < .08 && t.elevation.value() < .58 &&
                        t.biome != BiomeType::Taiga &&
                        t.biome != BiomeType::Tundra && (patch % 5 < 3);
                    surfaces_[std::size_t(y) * width() + x] =
                        beach ? CityTileType::Beach : CityTileType::Coast;
                }
            }
        }

    private:
        std::vector<CityTileType> surfaces_;
    };
} // namespace Paladin
