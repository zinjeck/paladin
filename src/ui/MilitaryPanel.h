#pragma once
#include "core/StrongId.h"
#include "ui/PanelDrag.h"
#include "ui/UiTypes.h"
#include <array>
#include <optional>
#include <string>
#include <vector>
union SDL_Event;
namespace Paladin
{
    class World;
    class Renderer;
    class GrayUiRenderer;
    class SceneSpriteLibrary;
    // Presentation owns only selection and input state. All recruitment, supply
    // and movement rules remain in MilitarySystem, including ownership checks.
    class MilitaryPanel
    {
    public:
        enum class Action
        {
            Close,
            New,
            NewGarrison,
            AddOne,
            RemoveOne,
            GarrisonAddOne,
            GarrisonRemoveOne,
            ToField,
            ToGarrison,
            Disband,
            Focus,
            Row
        };
        // Shared hit rectangles for input/accessibility and routing
        // regressions.
        std::optional<UiRectangle> controlBounds(
            Action,
            ArmyId unit = {}
        ) const noexcept;
        void toggle(SettlementId city);
        void close() noexcept;
        bool isOpen() const noexcept
        {
            return open_;
        }
        bool contains(float x, float y) const noexcept
        {
            return open_ && bounds_.contains(x, y);
        }
        void layout(int width, int height, const World&, RealmId);
        bool handle(const SDL_Event&, World&, RealmId);
        void render(
            Renderer&,
            const GrayUiRenderer&,
            const World&,
            RealmId,
            const SceneSpriteLibrary* artwork = nullptr
        ) const;
        ArmyId takeFocus() noexcept
        {
            const auto id = focus_;
            focus_ = {};
            return id;
        }
        ArmyId selection() const noexcept
        {
            return selected_;
        }
        const UiRectangle& bounds() const noexcept
        {
            return bounds_;
        }

    private:
        PanelDrag drag_;
        struct Control
        {
            UiRectangle bounds;
            Action action;
            std::string text;
            bool enabled = true;
            ArmyId unit;
        };
        void act(const Control&, World&, RealmId);
        std::vector<Control> controls_;
        std::optional<Control> pressedControl_;
        struct Pane
        {
            UiRectangle rows, track, thumb;
            int scroll = 0, maximum = 0;
        };
        std::array<Pane, 2> panes_;
        UiRectangle bounds_;
        int draggingScroll_ = -1;
        float scrollGrab_ = 0;
        SettlementId city_;
        ArmyId selected_, focus_;
        bool open_ = false, captured_ = false;
        int hovered_ = -1, pressed_ = -1;
        int viewportWidth_ = 0, viewportHeight_ = 0;
        std::string message_;
    };
} // namespace Paladin
