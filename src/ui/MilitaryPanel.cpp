#include "ui/MilitaryPanel.h"
#include "rendering/WorldArmyPresentation.h"
#include "simulation/MilitarySystem.h"
#include "ui/GrayUiRenderer.h"
#include "world/World.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    void MilitaryPanel::toggle(SettlementId city)
    {
        if (open_ && city_ == city)
        {
            close();
            return;
        }
        open_ = true;
        city_ = city;
        selected_ = {};
        panes_ = {};
        message_.clear();
        captured_ = false;
        draggingScroll_ = -1;
        pressed_ = hovered_ = -1;
        pressedControl_.reset();
    }
    void MilitaryPanel::close() noexcept
    {
        drag_.cancel();
        open_ = false;
        captured_ = false;
        draggingScroll_ = -1;
        pressed_ = hovered_ = -1;
        pressedControl_.reset();
        controls_.clear();
    }
    std::optional<UiRectangle> MilitaryPanel::controlBounds(
        Action action,
        ArmyId unit
    ) const noexcept
    {
        for (const auto& c : controls_)
        {
            if (c.action == action && (action != Action::Row || c.unit == unit))
            {
                return c.bounds;
            }
        }
        return {};
    }
    void MilitaryPanel::layout(
        int width,
        int height,
        const World& world,
        RealmId actor
    )
    {
        viewportWidth_ = width;
        viewportHeight_ = height;
        if (!open_)
        {
            return;
        }
        controls_.clear();
        const float w = std::min(760.F, std::max(0.F, float(width) - 16));
        const float h = std::min(560.F, std::max(0.F, float(height) - 32));
        bounds_ = drag_.place(
            {(width - w) * .5F, (height - h) * .5F, w, h},
            width,
            height
        );
        const float x = bounds_.x + 16, y = bounds_.y;
        const auto button = [&](UiRectangle b,
                                Action a,
                                std::string text,
                                bool enabled = true,
                                ArmyId id = ArmyId{})
        { controls_.push_back({b, a, std::move(text), enabled, id}); };
        button({bounds_.x + w - 44, y + 10, 28, 26}, Action::Close, "X");
        if (width < 560 || height < 432)
        {
            return;
        }
        const auto* city = world.settlement(city_);
        const bool owned = actor && city && city->ownerRealmId() == actor;
        const auto* selected = world.army(selected_);
        if (!selected || selected->ownerRealmId() != actor)
        {
            selected_ = {};
            selected = nullptr;
        }
        const float side = (w - 88) * .5F;
        constexpr float cardWidth = 76, cardHeight = 88, gap = 6;
        const int columns =
            std::max(1, int((side - 16 + gap) / (cardWidth + gap)));
        const int visibleRows = h >= 510 ? 3 : h >= 450 ? 2 : 1;
        const auto reserve = MilitarySystem::reserves(world, actor);
        for (int pane = 0; pane < 2; ++pane)
        {
            auto& view = panes_[pane];
            const float px = x + pane * (side + 56);
            view.rows =
                {px, y + 94, side - 16, visibleRows * (cardHeight + gap) - gap};
            std::vector<ArmyId> units;
            for (const auto& unit : world.armies())
            {
                if (unit.ownerRealmId() == actor &&
                    (pane == 0 ? !unit.garrisoned()
                               : unit.garrisonSettlementId() == city_))
                {
                    units.push_back(unit.id());
                }
            }
            const int totalRows = (int(units.size()) + columns - 1) / columns;
            view.maximum = std::max(0, totalRows - visibleRows);
            view.scroll = std::clamp(view.scroll, 0, view.maximum);
            const int first = view.scroll * columns;
            for (int i = first;
                 i < std::min(int(units.size()), first + visibleRows * columns);
                 ++i)
            {
                const int index = i - first;
                button(
                    {px + (index % columns) * (cardWidth + gap),
                     view.rows.y + (index / columns) * (cardHeight + gap),
                     cardWidth,
                     cardHeight},
                    Action::Row,
                    "",
                    true,
                    units[i]
                );
            }
            view.track = {px + side - 12, view.rows.y, 12, view.rows.height};
            const float thumb = view.maximum
                                    ? std::max(
                                          18.F,
                                          view.rows.height * visibleRows /
                                              std::max(1.F, float(totalRows))
                                      )
                                    : view.rows.height;
            view.thumb = {
                view.track.x,
                view.track.y + (view.rows.height - thumb) * view.scroll /
                                   std::max(1, view.maximum),
                12,
                thumb
            };
            const float tools = view.rows.y + view.rows.height + 12;
            const bool matching =
                selected &&
                (pane == 0 ? !selected->garrisoned()
                           : selected->garrisonSettlementId() == city_);
            const bool organize =
                matching && MilitarySystem::canOrganize(world, *selected);
            button(
                {px, tools, side, 28},
                pane == 0 ? Action::New : Action::NewGarrison,
                "New unit",
                owned && reserve > 0
            );
            button(
                {px, tools + 34, (side - 6) * .5F, 28},
                pane == 0 ? Action::AddOne : Action::GarrisonAddOne,
                "+1 soldier",
                organize && reserve > 0
            );
            button(
                {px + (side + 6) * .5F, tools + 34, (side - 6) * .5F, 28},
                pane == 0 ? Action::RemoveOne : Action::GarrisonRemoveOne,
                "-1 soldier",
                organize && MilitarySystem::releasable(world, *selected) > 0
            );
        }
        button(
            {x + side + 10, y + 180, 36, 32},
            Action::ToGarrison,
            ">",
            owned && selected && !selected->garrisoned() &&
                !selected->engagedOpponent() &&
                MilitarySystem::presentAt(world, *selected, city_)
        );
        button(
            {x + side + 10, y + 220, 36, 32},
            Action::ToField,
            "<",
            owned && selected && selected->garrisonSettlementId() == city_ &&
                !selected->engagedOpponent()
        );
        const float foot = y + h - 78;
        button(
            {x, foot, 130, 26},
            Action::Focus,
            "Show on map",
            selected && !selected->garrisoned() && selected->soldierCount() > 0
        );
        button(
            {x + 138, foot, 130, 26},
            Action::Disband,
            "Disband",
            selected && !selected->engagedOpponent()
        );
    }
    void MilitaryPanel::act(const Control& control, World& world, RealmId actor)
    {
        if (!control.enabled)
        {
            return;
        }
        using A = Action;
        switch (control.action)
        {
        case A::Close:
            close();
            return;
        case A::Row:
            selected_ = control.unit;
            message_.clear();
            break;
        case A::New:
        case A::NewGarrison:
        {
            const bool garrison = control.action == A::NewGarrison;
            selected_ =
                MilitarySystem::createUnit(world, actor, city_, garrison);
            message_ = selected_ ? "Unit formed from one employed reserve."
                                 : "Employ soldiers through your barracks. An "
                                   "owned city is required.";
            panes_[garrison ? 1 : 0].scroll = std::numeric_limits<int>::max();
            break;
        }
        case A::ToField:
        case A::ToGarrison:
            message_ = militaryResultText(
                MilitarySystem::setGarrison(
                    world,
                    actor,
                    selected_,
                    control.action == A::ToGarrison ? city_ : SettlementId{}
                )
            );
            panes_[control.action == A::ToGarrison ? 1 : 0].scroll =
                std::numeric_limits<int>::max();
            break;
        case A::AddOne:
        case A::GarrisonAddOne:
        case A::RemoveOne:
        case A::GarrisonRemoveOne:
            message_ = militaryResultText(
                MilitarySystem::resizeUnit(
                    world,
                    actor,
                    selected_,
                    control.action == A::AddOne ||
                            control.action == A::GarrisonAddOne
                        ? 1
                        : -1
                )
            );
            break;
        case A::Disband:
            message_ = militaryResultText(
                MilitarySystem::disbandUnit(world, actor, selected_)
            );
            if (!world.army(selected_))
            {
                selected_ = {};
            }
            break;
        case A::Focus:
            focus_ = selected_;
            close();
            return;
        }
        layout(viewportWidth_, viewportHeight_, world, actor);
    }
    bool MilitaryPanel::handle(
        const SDL_Event& event,
        World& world,
        RealmId actor
    )
    {
        if (!open_)
        {
            return false;
        }
        if (drag_.handle(event, bounds_))
        {
            captured_ = false;
            draggingScroll_ = -1;
            pressedControl_.reset();
            pressed_ = -1;
            layout(viewportWidth_, viewportHeight_, world, actor);
            return true;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            captured_ = false;
            draggingScroll_ = -1;
            pressedControl_.reset();
            pressed_ = -1;
            return false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            close();
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL &&
            contains(event.wheel.mouse_x, event.wheel.mouse_y))
        {
            for (auto& p : panes_)
            {
                if (p.rows.contains(event.wheel.mouse_x, event.wheel.mouse_y) ||
                    p.track.contains(event.wheel.mouse_x, event.wheel.mouse_y))
                {
                    p.scroll -= int(event.wheel.y);
                }
            }
            layout(viewportWidth_, viewportHeight_, world, actor);
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (draggingScroll_ >= 0)
            {
                auto& p = panes_[draggingScroll_];
                const float travel = p.track.height - p.thumb.height;
                if (travel > 0)
                {
                    p.scroll = int(std::round(
                        (event.motion.y - p.track.y - scrollGrab_) / travel *
                        p.maximum
                    ));
                }
                layout(viewportWidth_, viewportHeight_, world, actor);
                return true;
            }
            hovered_ = -1;
            for (int i = 0; i < int(controls_.size()); ++i)
            {
                if (controls_[i]
                        .bounds.contains(event.motion.x, event.motion.y))
                {
                    hovered_ = i;
                    break;
                }
            }
            return captured_ || contains(event.motion.x, event.motion.y);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (!contains(event.button.x, event.button.y))
            {
                return false;
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                close();
                return true;
            }
            if (event.button.button != SDL_BUTTON_LEFT)
            {
                return true;
            }
            captured_ = true;
            pressed_ = -1;
            pressedControl_.reset();
            for (int i = 0; i < 2; ++i)
            {
                auto& p = panes_[i];
                if (p.maximum > 0 &&
                    p.track.contains(event.button.x, event.button.y))
                {
                    draggingScroll_ = i;
                    scrollGrab_ =
                        p.thumb.contains(event.button.x, event.button.y)
                            ? event.button.y - p.thumb.y
                            : p.thumb.height * .5F;
                    const float travel = p.track.height - p.thumb.height;
                    if (travel > 0)
                    {
                        p.scroll = int(std::round(
                            (event.button.y - p.track.y - scrollGrab_) /
                            travel * p.maximum
                        ));
                    }
                    layout(viewportWidth_, viewportHeight_, world, actor);
                    return true;
                }
            }
            for (int i = 0; i < int(controls_.size()); ++i)
            {
                if (controls_[i].enabled && controls_[i].bounds.contains(
                                                event.button.x,
                                                event.button.y
                                            ))
                {
                    pressed_ = i;
                    pressedControl_ = controls_[i];
                    break;
                }
            }
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const bool consume =
                captured_ || contains(event.button.x, event.button.y);
            const auto pressed = pressedControl_;
            captured_ = false;
            draggingScroll_ = -1;
            pressed_ = -1;
            pressedControl_.reset();
            if (pressed)
            {
                for (const auto& c : controls_)
                {
                    if (c.enabled && c.action == pressed->action &&
                        c.unit == pressed->unit &&
                        c.bounds.contains(event.button.x, event.button.y))
                    {
                        const auto copy = c;
                        act(copy, world, actor);
                        break;
                    }
                }
            }
            return consume;
        }
        return false;
    }
    void MilitaryPanel::render(
        Renderer& renderer,
        const GrayUiRenderer& ui,
        const World& world,
        RealmId actor,
        const SceneSpriteLibrary* artwork
    ) const
    {
        if (!open_)
        {
            return;
        }
        ui.drawPanel(renderer, bounds_);
        const float x = bounds_.x + 16, y = bounds_.y,
                    inner = bounds_.width - 32;
        BitmapFontRenderer font;
        const auto label = [&](std::string text,
                               float px,
                               float py,
                               float width,
                               float scale = 2.F)
        {
            if (font.measureWidth(text, scale) > width)
            {
                while (!text.empty() &&
                       font.measureWidth(text + "...", scale) > width)
                {
                    text.pop_back();
                }
                text += "...";
            }
            ui.drawLabel(renderer, text, px, py, scale);
        };
        label("Military", x, y + 14, inner - 36, 3);
        const bool compact = viewportWidth_ < 560 || viewportHeight_ < 432;
        if (compact)
        {
            label("Enlarge the window to manage units.", x, y + 58, inner, 1);
        }
        else
        {
            const float side = (bounds_.width - 88) * .5F;
            const auto* city = world.settlement(city_);
            label("Field", x, y + 48, side, 2.5F);
            label("Garrison", x + side + 56, y + 48, side, 2.5F);
            label(
                "Reserve: " +
                    std::to_string(MilitarySystem::reserves(world, actor)) +
                    " across your realm",
                x,
                y + 76,
                side,
                1
            );
            label(
                (city ? std::string(city->name()) : "No city") +
                    " | Reserve: " +
                    std::to_string(MilitarySystem::available(world, city_)),
                x + side + 56,
                y + 76,
                side,
                1
            );
            for (const auto& p : panes_)
            {
                if (p.maximum > 0)
                {
                    renderer.fillRectangle(
                        p.track.x,
                        p.track.y,
                        p.track.width,
                        p.track.height,
                        {39, 39, 43, 255}
                    );
                    renderer.fillRectangle(
                        p.thumb.x,
                        p.thumb.y,
                        p.thumb.width,
                        p.thumb.height,
                        {148, 148, 155, 255}
                    );
                }
            }
            label(
                "Recruit through Employment. Select a unit to change its "
                "soldiers.",
                x,
                y + bounds_.height - 44,
                inner,
                1
            );
            label(
                "Field units must reach this city before joining its garrison.",
                x,
                y + bounds_.height - 31,
                inner,
                1
            );
            label(message_, x, y + bounds_.height - 18, inner, 1);
        }
        for (int i = 0; i < int(controls_.size()); ++i)
        {
            const auto& c = controls_[i];
            ui.drawButton(
                renderer,
                c.bounds,
                c.text,
                i == hovered_,
                i == pressed_,
                c.action == Action::Row && c.unit == selected_,
                c.enabled
            );
            if (c.action != Action::Row)
            {
                continue;
            }
            const auto* unit = world.army(c.unit);
            if (!unit)
            {
                continue;
            }
            label(
                unit->name(),
                c.bounds.x + 6,
                c.bounds.y + 8,
                c.bounds.width - 12,
                1
            );
            if (const auto* sprite =
                    artwork ? worldArmySprite(*artwork, world, *unit) : nullptr;
                sprite && sprite->texture)
            {
                const auto frame = artwork->frame(*sprite, false);
                constexpr float height = 36;
                const float width =
                    height * float(sprite->width / sprite->height);
                renderer.drawTexture(
                    *sprite->texture,
                    frame.x,
                    frame.y,
                    frame.width,
                    frame.height,
                    c.bounds.x + (c.bounds.width - width) * .5F,
                    c.bounds.y + 24,
                    width,
                    height
                );
            }
            const auto number = std::to_string(unit->soldierCount());
            const float scale =
                font.measureWidth(number, 2) <= c.bounds.width - 12 ? 2.F : 1.F;
            ui.drawLabel(
                renderer,
                number,
                c.bounds.x +
                    (c.bounds.width - font.measureWidth(number, scale)) * .5F,
                c.bounds.y + 66,
                scale
            );
        }
    }
} // namespace Paladin
