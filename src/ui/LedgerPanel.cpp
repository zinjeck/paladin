#include "ui/LedgerPanel.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/GrayUiRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Paladin
{
    namespace
    {
        constexpr float rowHeight = 28;
        const std::vector<std::string>
            cityPages{"Citizens", "Production", "Graphs", "Workplaces"};
        const std::vector<std::string>
            worldPages{"Realms", "Settlements", "Graphs", "Politics"};
    } // namespace
    void LedgerPanel::label(
        Renderer& r,
        const GrayUiRenderer&,
        std::string text,
        UiRectangle box,
        float,
        RenderColor color
    ) const
    {
        const float available = std::max(0.0F, box.width - 12);
        if (font_.measureWidth(text) > available)
        {
            while (!text.empty() &&
                   font_.measureWidth(text + "...") > available)
            {
                text.pop_back();
            }
            text += "...";
        }
        font_.drawText(
            r,
            text,
            box.x + 6,
            box.y + (box.height - 20) * .5F,
            color
        );
    }
    void LedgerPanel::toggle(bool events, bool world, SettlementId city)
    {
        const bool same =
            open_ && events_ == events && world_ == world && city_ == city;
        open_ = !same;
        events_ = events;
        world_ = world;
        city_ = city;
        page_ = 0;
        offset_ = 0;
        sortColumn_ = 0;
        descending_ = false;
        captured_ = false;
        observedVersion_ = ~std::uint64_t(0);
    }
    void LedgerPanel::layout(int width, int height)
    {
        const float w = std::max(
            200.F,
            std::min(events_ ? 580.F : 1060.F, float(width) - 32)
        );
        const float h = std::max(
            180.F,
            std::min(events_ ? 410.F : 580.F, float(height) - 130)
        );
        bounds_ = {events_ ? float(width) - w - 16 : (width - w) / 2, 80, w, h};
        close_ = {bounds_.x + w - 36, bounds_.y + 8, 28, 28};
        content_ = {bounds_.x + 12, bounds_.y + 74, w - 24, h - 130};
        columns_.clear();
        for (std::size_t i = 0; i < headings_.size(); ++i)
        {
            columns_.push_back(
                {content_.x + content_.width * i / headings_.size(),
                 content_.y,
                 content_.width / headings_.size(),
                 rowHeight}
            );
        }
        tabs_.clear();
        const float tabWidth = (w - 100) / 4;
        for (int i = 0; i < 4; ++i)
        {
            tabs_.push_back(
                {bounds_.x + 50 + i * tabWidth,
                 bounds_.y + h - 40,
                 tabWidth - 4,
                 28}
            );
        }
        previous_ = {bounds_.x + 12, bounds_.y + h - 40, 28, 28};
        next_ = {bounds_.x + w - 40, bounds_.y + h - 40, 28, 28};
    }
    int LedgerPanel::visibleRows() const
    {
        return std::max(
            1,
            int(content_.height / (events_ ? 48 : rowHeight)) - 1
        );
    }
    void LedgerPanel::changePage(int page)
    {
        page_ = std::clamp(page, 0, 3);
        offset_ = 0;
        sortColumn_ = 0;
        descending_ = false;
        observedVersion_ = ~std::uint64_t(0);
    }
    void LedgerPanel::sortRows()
    {
        if (headings_.empty())
        {
            return;
        }
        sortColumn_ = std::clamp(sortColumn_, 0, int(headings_.size()) - 1);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [&](const auto& a, const auto& b)
            {
                const auto& x = a.cells[sortColumn_];
                const auto& y = b.cells[sortColumn_];
                if (x.numeric != y.numeric)
                {
                    return x.numeric;
                }
                if (x.numeric && y.numeric)
                {
                    if (x.number != y.number)
                    {
                        return descending_ ? x.number > y.number
                                           : x.number < y.number;
                    }
                }
                else if (x.text != y.text)
                {
                    return descending_ ? x.text > y.text : x.text < y.text;
                }
                return a.id < b.id;
            }
        );
    }
    bool LedgerPanel::handle(const SDL_Event& e)
    {
        if (!open_)
        {
            return false;
        }
        if ((e.type == SDL_EVENT_KEY_DOWN &&
             e.key.scancode == SDL_SCANCODE_ESCAPE) ||
            (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
             e.button.button == SDL_BUTTON_RIGHT))
        {
            close();
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_WHEEL &&
            containsPoint(e.wheel.mouse_x, e.wheel.mouse_y))
        {
            const float direction = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                        ? -e.wheel.y
                                        : e.wheel.y;
            offset_ = std::clamp(
                offset_ - int(direction * 3),
                0,
                std::max(0, int(rows_.size()) - visibleRows())
            );
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION)
        {
            return captured_ || containsPoint(e.motion.x, e.motion.y);
        }
        const auto hit = [&](float x, float y)
        {
            if (close_.contains(x, y))
            {
                return 100;
            }
            if (!events_)
            {
                if (previous_.contains(x, y))
                {
                    return 101;
                }
                if (next_.contains(x, y))
                {
                    return 102;
                }
                for (int i = 0; i < int(tabs_.size()); ++i)
                {
                    if (tabs_[i].contains(x, y))
                    {
                        return 110 + i;
                    }
                }
                if (page_ != 2)
                {
                    for (int i = 0; i < int(columns_.size()); ++i)
                    {
                        if (columns_[i].contains(x, y))
                        {
                            return i;
                        }
                    }
                }
            }
            return -1;
        };
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e.button.button == SDL_BUTTON_LEFT)
        {
            captured_ = containsPoint(e.button.x, e.button.y);
            pressed_ = hit(e.button.x, e.button.y);
            return captured_;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            e.button.button == SDL_BUTTON_LEFT && captured_)
        {
            captured_ = false;
            const int action = hit(e.button.x, e.button.y);
            if (action == pressed_)
            {
                if (action == 100)
                {
                    close();
                }
                else if (action == 101)
                {
                    changePage(page_ - 1);
                }
                else if (action == 102)
                {
                    changePage(page_ + 1);
                }
                else if (action >= 110)
                {
                    changePage(action - 110);
                }
                else if (action >= 0)
                {
                    descending_ = sortColumn_ == action ? !descending_ : false;
                    sortColumn_ = action;
                    sortRows();
                    offset_ = 0;
                }
            }
            return true;
        }
        return false;
    }
    void LedgerPanel::refresh(const Simulation& simulation)
    {
        if (!open_ || observedVersion_ == simulation.reports.version())
        {
            return;
        }
        observedVersion_ = simulation.reports.version();
        buildRows(simulation);
        if (!events_)
        {
            sortRows();
        }
        offset_ = std::clamp(
            offset_,
            0,
            std::max(0, int(rows_.size()) - visibleRows())
        );
    }
    void LedgerPanel::render(Renderer& r, const GrayUiRenderer& ui) const
    {
        if (!open_)
        {
            return;
        }
        ui.drawPanel(r, bounds_);
        label(
            r,
            ui,
            title_,
            {bounds_.x + 10, bounds_.y + 8, bounds_.width - 60, 30},
            2
        );
        label(
            r,
            ui,
            subtitle_,
            {bounds_.x + 10, bounds_.y + 42, bounds_.width - 20, 24},
            1.2F,
            {185, 188, 194, 255}
        );
        const auto button = [&](const UiRectangle& box,
                                const std::string& text,
                                bool selected,
                                bool enabled)
        {
            ui.drawButton(r, box, "", false, false, selected, enabled);
            label(
                r,
                ui,
                text,
                box,
                1.5F,
                enabled ? RenderColor{235, 235, 238, 255}
                        : RenderColor{135, 138, 145, 255}
            );
        };
        button(close_, "X", false, true);
        if (!events_ && page_ == 2)
        {
            constexpr const char* names[]{
                "Population",
                "Treasury (gold)",
                "Average health",
                "Food in containers"
            };
            const auto value = [](const ReportSample& s, int i)
            {
                return i == 0   ? s.population
                       : i == 1 ? s.gold
                       : i == 2 ? s.health
                                : s.food;
            };
            for (int graph = 0; graph < 4; ++graph)
            {
                UiRectangle b{
                    content_.x + (graph % 2) * content_.width / 2,
                    content_.y + (graph / 2) * content_.height / 2,
                    content_.width / 2 - 14,
                    content_.height / 2 - 12
                };
                label(r, ui, names[graph], {b.x, b.y, b.width, 22});
                double maximum = 1, minimum = 0;
                for (const auto& s : samples_)
                {
                    maximum = std::max(maximum, value(s, graph));
                    minimum = std::min(minimum, value(s, graph));
                }
                r.drawLine(
                    b.x + 35,
                    b.y + 28,
                    b.x + 35,
                    b.y + b.height - 24,
                    {135, 138, 145, 255}
                );
                r.drawLine(
                    b.x + 35,
                    b.y + b.height - 24,
                    b.x + b.width,
                    b.y + b.height - 24,
                    {135, 138, 145, 255}
                );
                label(
                    r,
                    ui,
                    std::to_string(int(maximum)),
                    {b.x, b.y + 20, 65, 20},
                    1
                );
                label(
                    r,
                    ui,
                    std::to_string(int(minimum)),
                    {b.x, b.y + b.height - 35, 65, 20},
                    1
                );
                if (samples_.size() < 2)
                {
                    label(
                        r,
                        ui,
                        "Collecting history...",
                        {b.x + 40, b.y + 55, b.width - 40, 24},
                        1.2F
                    );
                }
                for (std::size_t i = 1; i < samples_.size(); ++i)
                {
                    const auto x = [&](const auto& s)
                    {
                        return b.x + 35 +
                               float(
                                   (s.minute - samples_.front().minute) /
                                   std::max(
                                       1.0,
                                       samples_.back().minute -
                                           samples_.front().minute
                                   )
                               ) * (b.width - 35);
                    };
                    const auto y = [&](const auto& s)
                    {
                        return b.y + b.height - 24 -
                               float(
                                   (value(s, graph) - minimum) /
                                   (maximum - minimum)
                               ) * (b.height - 54);
                    };
                    r.drawLine(
                        x(samples_[i - 1]),
                        y(samples_[i - 1]),
                        x(samples_[i]),
                        y(samples_[i]),
                        {112, 202, 170, 255}
                    );
                }
                if (!samples_.empty())
                {
                    label(
                        r,
                        ui,
                        "Day " +
                            std::to_string(
                                int(samples_.front().minute / 1440) + 1
                            ) +
                            " to " +
                            std::to_string(
                                int(samples_.back().minute / 1440) + 1
                            ),
                        {b.x + 35, b.y + b.height - 20, b.width - 35, 20},
                        1
                    );
                }
            }
        }
        else
        {
            if (!events_)
            {
                for (std::size_t i = 0; i < columns_.size(); ++i)
                {
                    button(
                        columns_[i],
                        headings_[i] + (int(i) == sortColumn_
                                            ? (descending_ ? " v" : " ^")
                                            : ""),
                        int(i) == sortColumn_,
                        true
                    );
                }
            }
            if (rows_.empty())
            {
                label(
                    r,
                    ui,
                    events_ ? "No events recorded." : "No entries yet.",
                    {content_.x, content_.y + 32, content_.width, 30}
                );
            }
            for (int row = 0;
                 row < visibleRows() && row + offset_ < int(rows_.size());
                 ++row)
            {
                const auto& entry = rows_[row + offset_];
                const float y =
                    content_.y + rowHeight + row * (events_ ? 48 : rowHeight);
                if (row % 2 == 0)
                {
                    r.fillRectangle(
                        content_.x,
                        y,
                        content_.width,
                        events_ ? 48 : rowHeight,
                        {48, 50, 55, 255}
                    );
                }
                if (events_)
                {
                    label(
                        r,
                        ui,
                        entry.cells[0].text,
                        {content_.x, y, content_.width, 20},
                        1.2F,
                        {218, 193, 125, 255}
                    );
                    label(
                        r,
                        ui,
                        entry.cells[1].text,
                        {content_.x, y + 20, content_.width, 25},
                        1.2F
                    );
                }
                else
                {
                    for (std::size_t col = 0; col < entry.cells.size(); ++col)
                    {
                        label(
                            r,
                            ui,
                            entry.cells[col].text,
                            {columns_[col].x,
                             y,
                             columns_[col].width,
                             rowHeight},
                            1.2F
                        );
                    }
                }
            }
        }
        if (!events_)
        {
            const auto& pages = world_ ? worldPages : cityPages;
            button(previous_, "<", false, page_ > 0);
            button(next_, ">", false, page_ < 3);
            for (int i = 0; i < 4; ++i)
            {
                button(tabs_[i], pages[i], page_ == i, true);
            }
        }
    }
} // namespace Paladin
