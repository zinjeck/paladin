#pragma once
#include "core/StrongId.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include "simulation/BattleSystem.h"
#include "ui/UiButton.h"
#include <memory>
#include <vector>

namespace Paladin
{
    // Only deployment coordinates are local. Personnel always remain in World.
    struct BattleSoldierView
    {
        SoldierId soldier;
        double x = 0, y = 0;
        bool player = false, female = false;
    };

    class BattleScene
    {
    public:
        BattleScene(
            const World&,
            const BattleEncounter&,
            int width,
            int height
        );
        ~BattleScene();
        void render(Renderer&, double hour, double sun, double seconds);
        void update(double seconds, int width, int height);
        bool handle(const SDL_Event&, int width, int height);
        void layout(int height);
        const SettlementMap& map() const
        {
            return *map_;
        }
        const auto& soldiers() const
        {
            return soldiers_;
        }
        UiButton retreat{"Retreat"};

    private:
        void clamp(int width, int height);
        std::unique_ptr<SettlementMap> map_;
        std::vector<BattleSoldierView> soldiers_;
        CityRenderer renderer_;
        Camera2D camera_;
        TileRenderMetrics metrics_;
    };
} // namespace Paladin
