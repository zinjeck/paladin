#include "ui/EmploymentPanel.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace Paladin
{
    namespace
    {
        void icon(
            Renderer& renderer,
            const UiRectangle& box,
            std::string_view type
        )
        {
            const auto* d = SettlementObjectCatalog::definition(type);
            if (!d)
            {
                return;
            }
            const float scale = std::min(
                (box.width - 16) / d->visual.iconWidth,
                (box.height - 16) / d->visual.iconHeight
            );
            const float w = d->visual.iconWidth * scale,
                        h = d->visual.iconHeight * scale;
            const float x = box.x + (box.width - w) * .5F,
                        y = box.y + (box.height - h) * .5F;
            const auto& edge = d->visual.frameColor;
            const auto& fill = d->visual.fillColor;
            renderer
                .fillRectangle(x, y, w, h, {edge[0], edge[1], edge[2], 255});
            renderer.fillRectangle(
                x + 3,
                y + 3,
                w - 6,
                h - 6,
                {fill[0], fill[1], fill[2], 255}
            );
        }
        std::string percentage(double value)
        {
            std::ostringstream text;
            text << std::fixed << std::setprecision(1) << value << "%";
            return text.str();
        }
        void fitLabel(
            Renderer& renderer,
            const GrayUiRenderer& ui,
            std::string_view label,
            UiRectangle bounds
        )
        {
            const float size = std::min(
                1.65F,
                (bounds.width - 8) / std::max(1.0F, float(label.size()) * 6 - 1)
            );
            ui.drawLabel(renderer, label, bounds.x + 4, bounds.y + 7, size);
        }
        UiRectangle reformSection(
            Renderer& renderer,
            const GrayUiRenderer& ui,
            const UiRectangle& bounds,
            std::string_view title,
            std::string_view scope,
            int workHours,
            bool realm
        )
        {
            const RenderColor edge = realm ? RenderColor{153, 78, 80, 255}
                                           : RenderColor{88, 88, 95, 255};
            const RenderColor shadow = realm ? RenderColor{85, 47, 51, 255}
                                             : RenderColor{49, 49, 54, 255};
            renderer.fillRectangle(
                bounds.x,
                bounds.y,
                bounds.width,
                bounds.height,
                shadow
            );
            renderer.fillRectangle(
                bounds.x,
                bounds.y,
                bounds.width - 2,
                bounds.height - 2,
                edge
            );
            renderer.fillRectangle(
                bounds.x + 4,
                bounds.y + 4,
                bounds.width - 8,
                bounds.height - 8,
                {64, 64, 69, 255}
            );
            const BitmapFontRenderer font;
            const float titleScale = std::min(
                2.2F,
                (bounds.width - 24) / font.measureWidth(title, 1)
            );
            ui.drawLabel(
                renderer,
                title,
                bounds.x +
                    (bounds.width - font.measureWidth(title, titleScale)) * .5F,
                bounds.y + 18,
                titleScale
            );
            const float scopeScale = std::min(
                1.2F,
                (bounds.width - 24) / font.measureWidth(scope, 1)
            );
            ui.drawLabel(
                renderer,
                scope,
                bounds.x +
                    (bounds.width - font.measureWidth(scope, scopeScale)) * .5F,
                bounds.y + 45,
                scopeScale,
                {185, 185, 192, 255}
            );
            renderer.fillRectangle(
                bounds.x + 12,
                bounds.y + 65,
                bounds.width - 24,
                2,
                edge
            );
            constexpr float gap = 6;
            const float cellWidth = (bounds.width - 24 - gap) / 2;
            const float cellHeight = (bounds.height - 90 - 2 * gap) / 3;
            UiRectangle workDayCell;
            for (int i = 0; i < 6; ++i)
            {
                const UiRectangle cell{
                    bounds.x + 12 + (i % 2) * (cellWidth + gap),
                    bounds.y + 78 + (i / 2) * (cellHeight + gap),
                    cellWidth,
                    cellHeight
                };
                renderer.fillRectangle(
                    cell.x,
                    cell.y,
                    cell.width,
                    cell.height,
                    edge
                );
                renderer.fillRectangle(
                    cell.x + 3,
                    cell.y + 3,
                    cell.width - 6,
                    cell.height - 6,
                    {64, 64, 69, 255}
                );
                if (i == 0)
                {
                    workDayCell = cell;
                    fitLabel(
                        renderer,
                        ui,
                        "Work Day: " + std::to_string(workHours) + " Hours",
                        {cell.x + 4,
                         cell.y + 5,
                         cell.width - 8,
                         cell.height - 10}
                    );
                }
            }
            return workDayCell;
        }
    } // namespace
    bool EmploymentPanel::containsPoint(float x, float y) const noexcept
    {
        return open_ &&
               (bounds_.contains(x, y) ||
                (!selectedType_.empty() && listBounds_.contains(x, y)));
    }
    bool EmploymentPanel::pointerPressed(float x, float y)
    {
        pressed_ = -1;
        if (!containsPoint(x, y))
        {
            return false;
        }
        for (std::size_t i = 0; i < hits_.size(); ++i)
        {
            if (hits_[i].bounds.contains(x, y))
            {
                pressed_ = int(i);
                break;
            }
        }
        dragCandidate_ = true;
        pressX_ = x;
        pressY_ = y;
        dragX_ = x - positionX_;
        dragY_ = y - positionY_;
        return true;
    }
    bool EmploymentPanel::pointerMoved(float x, float y)
    {
        if (!dragging_ && dragCandidate_ &&
            std::hypot(x - pressX_, y - pressY_) >= 4)
        {
            dragging_ = true;
            pressed_ = -1;
        }
        if (!dragging_)
        {
            return false;
        }
        positionX_ = std::clamp(
            x - dragX_,
            64 - bounds_.width,
            std::max(64.0F, viewportWidth_ - 64)
        );
        positionY_ =
            std::clamp(y - dragY_, 0.0F, std::max(0.0F, viewportHeight_ - 40));
        return true;
    }
    void EmploymentPanel::pointerReleased(
        float x,
        float y,
        SettlementMap& map,
        SettlementCitizenState& citizens,
        double minute
    )
    {
        dragCandidate_ = false;
        if (dragging_)
        {
            dragging_ = false;
            pressed_ = -1;
            return;
        }
        if (!open_ || pressed_ < 0 || std::size_t(pressed_) >= hits_.size())
        {
            pressed_ = -1;
            return;
        }
        const auto hit = hits_[pressed_];
        pressed_ = -1;
        if (!hit.bounds.contains(x, y))
        {
            return;
        }
        if (hit.type == "close")
        {
            close();
            return;
        }
        if (hit.type == "foundSettlement")
        {
            foundSettlement_ = true;
            close();
            return;
        }
        if (worldMode_)
        {
            return;
        }
        if (hit.type == "realmWorkDay" || hit.type == "cityWorkDay")
        {
            workDayChange_ =
                WorkDayChange{hit.type == "realmWorkDay", hit.delta};
            return;
        }
        if (hit.icon)
        {
            selectedType_ = hit.type;
            scrollOffset_ = 0;
            return;
        }
        if (hit.type == "previous")
        {
            scrollOffset_ = scrollOffset_ > 0 ? scrollOffset_ - 1 : 0;
            return;
        }
        if (hit.type == "next")
        {
            ++scrollOffset_;
            return;
        }
        if (hit.workplace && hit.delta == 0)
        {
            focusedWorkplace_ = hit.workplace;
            close();
            return;
        }
        if (hit.workplace)
        {
            map.employment().adjust(hit.workplace, hit.delta, citizens);
        }
        else
        {
            map.employment().adjustType(hit.type, hit.delta, citizens);
        }
        map.employment().record(minute, citizens);
    }
    void EmploymentPanel::scroll(float amount)
    {
        if (!selectedType_.empty())
        {
            scrollOffset_ = amount > 0
                                ? (scrollOffset_ > 0 ? scrollOffset_ - 1 : 0)
                                : scrollOffset_ + 1;
        }
    }
    void EmploymentPanel::render(
        Renderer& renderer,
        const GrayUiRenderer& ui,
        const SettlementMap& map,
        const SettlementCitizenState& citizens,
        double minute
    )
    {
        if (!open_)
        {
            return;
        }
        hits_.clear();
        const bool laws = section_ == "Laws";
        const float width =
            std::min(730.0F, float(renderer.outputWidth()) * .62F);
        const float height =
            std::min(540.0F, float(renderer.outputHeight()) - 150);
        viewportWidth_ = float(renderer.outputWidth());
        viewportHeight_ = float(renderer.outputHeight());
        if (!positioned_)
        {
            positionX_ = std::max(8.0F, (viewportWidth_ - width - 292) * .5F);
            positionY_ = 110;
            positioned_ = true;
        }
        if (!dragging_)
        {
            // Ease back inside the viewport after release or a window resize.
            const float x = std::clamp(
                positionX_,
                8.0F,
                std::max(8.0F, viewportWidth_ - width - 8)
            );
            const float y = std::clamp(
                positionY_,
                8.0F,
                std::max(8.0F, viewportHeight_ - height - 8)
            );
            positionX_ = std::abs(x - positionX_) < 1
                             ? x
                             : positionX_ + (x - positionX_) * .35F;
            positionY_ = std::abs(y - positionY_) < 1
                             ? y
                             : positionY_ + (y - positionY_) * .35F;
        }
        const float left = positionX_;
        const float top = positionY_;
        bounds_ = {left, top, width, height};
        const float listWidth = std::min(284.0F, viewportWidth_ - 16);
        const float listX = left + width + 8 + listWidth <= viewportWidth_ - 8
                                ? left + width + 8
                                : std::max(8.0F, left - listWidth - 8);
        listBounds_ = {
            listX,
            std::clamp(top, 8.0F, std::max(8.0F, viewportHeight_ - height - 8)),
            listWidth,
            height
        };
        ui.drawPanel(renderer, bounds_);
        const auto button = [&](UiRectangle b,
                                std::string_view text,
                                Hit hit,
                                bool enabled = true)
        {
            ui.drawButton(
                renderer,
                b,
                text,
                false,
                false,
                false,
                enabled,
                hit.type + (hit.delta < 0   ? "-decrease"
                            : hit.delta > 0 ? "-increase"
                                            : "")
            );
            if (enabled)
            {
                hit.bounds = b;
                hits_.push_back(std::move(hit));
            }
        };
        const BitmapFontRenderer titleFont;
        const float titleScale =
            std::min(3.0F, (width - 100) / titleFont.measureWidth(section_, 1));
        ui.drawLabel(
            renderer,
            section_,
            left + (width - titleFont.measureWidth(section_, titleScale)) * .5F,
            top + 13,
            titleScale
        );
        button({left + width - 38, top + 6, 30, 28}, "X", {{}, "close"});
        if (worldMode_)
        {
            const float informationHeight = (height - 90) * .62F;
            ui.drawPanel(
                renderer,
                {left + 18, top + 54, width - 36, informationHeight}
            );
            const float rowY = top + 66 + informationHeight;
            const float cellWidth = (width - 52) / 5;
            for (int i = 0; i < 5; ++i)
            {
                const bool founding = section_ == "Economy" && i == 0;
                const UiRectangle cell{
                    left + 18 + i * (cellWidth + 4),
                    rowY,
                    cellWidth,
                    top + height - rowY - 18
                };
                button(
                    cell,
                    "",
                    {{}, founding ? "foundSettlement" : "emptyDecision"},
                    founding
                );
                if (founding)
                {
                    const float scale = std::min(
                        1.6F,
                        (cellWidth - 12) /
                            titleFont.measureWidth("Settlement", 1)
                    );
                    const std::array lines{"Found a", "New", "Settlement"};
                    for (int line = 0; line < 3; ++line)
                    {
                        ui.drawLabel(
                            renderer,
                            lines[line],
                            cell.x +
                                (cell.width -
                                 titleFont.measureWidth(lines[line], scale)) *
                                    .5F,
                            cell.y + cell.height * .5F + (line - 1) * 16 - 5,
                            scale
                        );
                    }
                }
            }
            return;
        }
        if (laws)
        {
            const float sideWidth = (width - 54) * .5F;
            const UiRectangle
                realm{left + 18, bounds_.y + 60, sideWidth, height - 78};
            const UiRectangle city{
                realm.x + sideWidth + 18,
                realm.y,
                sideWidth,
                realm.height
            };
            const auto hours = [](const CitizenSimulationPolicy& policy)
            { return (policy.shiftEndMinute - policy.shiftStartMinute) / 60; };
            const auto realmCell = reformSection(
                renderer,
                ui,
                realm,
                "REALM REFORMS",
                "ALL CONTROLLED SETTLEMENTS",
                realmWorkDayHours_,
                true
            );
            const auto cityCell = reformSection(
                renderer,
                ui,
                city,
                "CITY REFORMS",
                "THIS SETTLEMENT",
                hours(map.activities.policy),
                false
            );
            const auto controls = [&](const UiRectangle& cell,
                                      const std::string& type,
                                      int workHours)
            {
                const float side = std::min(28.0F, (cell.width - 24) / 2);
                const float y = cell.y + 34;
                button(
                    {cell.x + 8, y, side, 24},
                    "<",
                    {{}, type, {}, -1},
                    workHours > 0
                );
                button(
                    {cell.x + 12 + side, y, side, 24},
                    ">",
                    {{}, type, {}, 1},
                    workHours < 14
                );
            };
            controls(realmCell, "realmWorkDay", realmWorkDayHours_);
            controls(cityCell, "cityWorkDay", hours(map.activities.policy));
            return;
        }
        const bool population = section_ == "Population";
        if (section_ != "Employment" && !population)
        {
            return;
        }
        struct GraphSample
        {
            double gameMinute;
            double value;
        };
        std::vector<GraphSample> history;
        if (population)
        {
            for (const auto& sample : citizens.populationHistory())
            {
                history.push_back(
                    {sample.gameMinute, double(sample.population)}
                );
            }
        }
        else
        {
            for (const auto& sample : map.employment().history())
            {
                history.push_back(
                    {sample.gameMinute, sample.unemployedPercent}
                );
            }
        }
        const auto adults = std::count_if(
            citizens.citizens().begin(),
            citizens.citizens().end(),
            [](const auto& c) { return !c.child; }
        );
        const double percent =
            adults ? 100.0 * map.employment().unemployed(citizens) / adults : 0;
        font_.drawText(
            renderer,
            population
                ? "Population: " +
                      std::to_string(
                          map.logistics.founded() ? citizens.citizens().size()
                                                  : 0
                      ) +
                      "  |  Every 4 hours"
                : "Unemployed: " +
                      std::to_string(map.employment().unemployed(citizens)) +
                      " / " + std::to_string(adults) + "  (" +
                      percentage(percent) + ")",
            left + 18,
            top + 47
        );
        const float summaryWidth = (width - 112) / 3;
        const UiRectangle graph{
            left + 88 + summaryWidth,
            top + 98,
            summaryWidth * 2,
            std::clamp(height - 350.0F, 90.0F, 200.0F)
        };
        if (population)
        {
            double happiness = 0, health = 0, hunger = 0;
            std::size_t count = 0;
            for (const auto& citizen : citizens.citizens())
            {
                if (citizen.health <= 0 || !map.logistics.founded())
                {
                    continue;
                }
                happiness += citizen.happiness;
                health += citizen.health;
                hunger += citizen.hunger;
                ++count;
            }
            const std::array values{happiness, health, hunger};
            const std::array labels{"Happiness:", "Health:", "Hunger:"};
            for (int i = 0; i < 3; ++i)
            {
                const float rowHeight = graph.height / 3;
                const UiRectangle row{
                    left + 18,
                    graph.y + i * rowHeight,
                    summaryWidth,
                    rowHeight - 6
                };
                ui.drawPanel(renderer, row);
                const float labelSize =
                    std::min(1.35F, (row.width * .48F - 8) / 59);
                const float labelY = row.y + (row.height - 7 * labelSize) * .5F;
                ui.drawLabel(renderer, labels[i], row.x + 6, labelY, labelSize);
                const double value = count ? values[i] / count : 0;
                const double good =
                    i == 2 ? (value < 50 ? 100 : 100 - (value - 50) * 2)
                           : value;
                const RenderColor color =
                    good >= 75   ? RenderColor{100, 188, 100, 255}
                    : good >= 50 ? RenderColor{214, 190, 71, 255}
                    : good >= 25 ? RenderColor{225, 141, 54, 255}
                                 : RenderColor{205, 77, 72, 255};
                const UiRectangle meter{
                    row.x + row.width * .5F,
                    row.y + (row.height - 22) * .5F,
                    row.width * .5F - 8,
                    22
                };
                renderer.fillRectangle(
                    meter.x,
                    meter.y,
                    meter.width,
                    meter.height,
                    {std::uint8_t(color.red / 3),
                     std::uint8_t(color.green / 3),
                     std::uint8_t(color.blue / 3),
                     255}
                );
                const float fill =
                    meter.width * float(std::clamp(value / 100, 0.0, 1.0));
                renderer
                    .fillRectangle(meter.x, meter.y, fill, meter.height, color);
                const std::string text = count ? percentage(value) : "--";
                const float scale = std::min(
                    1.2F,
                    (meter.width - 4) / titleFont.measureWidth(text, 1)
                );
                ui.drawLabel(
                    renderer,
                    text,
                    meter.x +
                        (meter.width - titleFont.measureWidth(text, scale)) *
                            .5F,
                    meter.y + (meter.height - 7 * scale) * .5F,
                    scale
                );
            }
        }
        double maximum = 100;
        if (population)
        {
            maximum = 1;
            for (const auto& sample : history)
            {
                maximum = std::max(maximum, sample.value);
            }
            maximum = std::max(10.0, std::ceil(maximum / 10) * 10);
        }
        renderer.fillRectangle(
            graph.x,
            graph.y,
            graph.width,
            graph.height,
            {32, 35, 40, 255}
        );
        const int endDay = std::max(16, int(std::ceil(minute / 1440 + 1)));
        const int startDay = endDay - 15;
        for (int i = 0; i <= 10; ++i)
        {
            const float y = graph.y + graph.height * i / 10;
            renderer.drawLine(
                graph.x,
                y,
                graph.x + graph.width,
                y,
                i % 5 == 0 ? RenderColor{111, 117, 128, 255}
                           : RenderColor{61, 66, 74, 255}
            );
            if (i % 2 == 0)
            {
                ui.drawLabel(
                    renderer,
                    population ? std::to_string(int(maximum * (10 - i) / 10))
                               : std::to_string(100 - i * 10) + "%",
                    graph.x - 46,
                    y - 6,
                    2.0F,
                    {201, 204, 211, 255}
                );
            }
        }
        for (int i = 0; i <= 15; ++i)
        {
            const float x = graph.x + graph.width * i / 15;
            renderer.drawLine(
                x,
                graph.y,
                x,
                graph.y + graph.height,
                {61, 66, 74, 255}
            );
            ui.drawLabel(
                renderer,
                std::to_string(startDay + i),
                x - 4,
                graph.y + graph.height + 9,
                1.75F,
                {201, 204, 211, 255}
            );
        }
        ui.drawLabel(
            renderer,
            "Days",
            graph.x + graph.width * .5F - 16,
            graph.y + graph.height + 30,
            1.5F
        );
        const auto point = [&](const GraphSample& sample)
        {
            return std::pair{
                graph.x + float(std::clamp(
                              (sample.gameMinute / 1440 + 1 - startDay) / 15,
                              0.0,
                              1.0
                          )) * graph.width,
                graph.y + float(1 - sample.value / maximum) * graph.height
            };
        };
        for (std::size_t i = 1; i < history.size(); ++i)
        {
            if (history[i].gameMinute / 1440 + 1 < startDay)
            {
                continue;
            }
            const auto a = point(history[i - 1]), b = point(history[i]);
            renderer.drawLine(
                a.first,
                a.second,
                b.first,
                a.second,
                {240, 65, 72, 255}
            );
            renderer.drawLine(
                a.first,
                a.second + 1,
                b.first,
                a.second + 1,
                {240, 65, 72, 255}
            );
            renderer.drawLine(
                b.first,
                a.second,
                b.first,
                b.second,
                {240, 65, 72, 255}
            );
        }
        if (!history.empty())
        {
            const auto a = point(history.back()),
                       b = point(
                           {minute, population ? history.back().value : percent}
                       );
            renderer.drawLine(
                a.first,
                a.second,
                b.first,
                b.second,
                {240, 65, 72, 255}
            );
            renderer.fillRectangle(
                b.first - 2,
                b.second - 2,
                4,
                4,
                {255, 107, 111, 255}
            );
        }
        if (population)
        {
            for (const auto& sample : history)
            {
                if (sample.gameMinute / 1440 + 1 < startDay)
                {
                    continue;
                }
                const auto [x, y] = point(sample);
                renderer
                    .fillRectangle(x - 1, y - 1, 3, 3, {255, 107, 111, 255});
            }
            return;
        }
        const float cardsTop = graph.y + graph.height + 53;
        const float cardWidth = (width - 48) / 3;
        const float cardHeight =
            std::max(65.0F, (bounds_.y + height - cardsTop - 16) / 2);
        std::size_t index = 0;
        for (const auto& d : workplaceDefinitions())
        {
            const UiRectangle card{
                left + 12 + float(index % 3) * (cardWidth + 12),
                cardsTop + float(index / 3) * cardHeight,
                cardWidth,
                cardHeight - 6
            };
            ui.drawButton(
                renderer,
                card,
                "",
                false,
                false,
                selectedType_ == d.objectTypeId,
                true
            );
            const UiRectangle iconBox{card.x + 8, card.y + 5, 43, 40};
            if (selectedType_ == d.objectTypeId)
            {
                renderer.fillRectangle(
                    iconBox.x,
                    iconBox.y,
                    iconBox.width,
                    iconBox.height,
                    {144, 149, 158, 255}
                );
            }
            icon(renderer, iconBox, d.objectTypeId);
            std::size_t count = 0, capacity = 0, maximumCapacity = 0;
            for (const auto& w : map.employment().workplaces())
            {
                if (w.objectTypeId == d.objectTypeId)
                {
                    count += map.employment().employed(w.id, citizens);
                    capacity += w.capacity;
                    if (w.operational)
                    {
                        maximumCapacity += w.maximumCapacity;
                    }
                }
            }
            fitLabel(
                renderer,
                ui,
                std::to_string(count) + "/" + std::to_string(capacity),
                {card.x + 61, card.y + 13, card.width - 66, 22}
            );
            button(
                {card.x + 8, card.y + card.height - 27, 28, 23},
                "<",
                {{}, std::string(d.objectTypeId), {}, -1},
                count > 0
            );
            button(
                {card.x + 39, card.y + card.height - 27, 28, 23},
                ">",
                {{}, std::string(d.objectTypeId), {}, 1},
                capacity < maximumCapacity &&
                    map.employment().unemployed(citizens) > 0
            );
            hits_.push_back({card, std::string(d.objectTypeId), {}, 0, true});
            ++index;
        }
        if (selectedType_.empty())
        {
            return;
        }
        ui.drawPanel(renderer, listBounds_);
        const auto* d = SettlementObjectCatalog::definition(selectedType_);
        const std::string_view listTitle = d ? d->displayName : selectedType_;
        const float listTitleSize = std::min(
            2.2F,
            (listBounds_.width - 24) / titleFont.measureWidth(listTitle, 1)
        );
        ui.drawLabel(
            renderer,
            listTitle,
            listBounds_.x + (listBounds_.width -
                             titleFont.measureWidth(listTitle, listTitleSize)) *
                                .5F,
            listBounds_.y + 15,
            listTitleSize
        );
        std::vector<const Workplace*> locations;
        for (const auto& w : map.employment().workplaces())
        {
            if (w.objectTypeId == selectedType_)
            {
                locations.push_back(&w);
            }
        }
        const std::size_t visible = std::max(1, int((height - 105) / 74));
        scrollOffset_ = std::min(
            scrollOffset_,
            locations.size() > visible ? locations.size() - visible : 0
        );
        if (locations.empty())
        {
            fitLabel(
                renderer,
                ui,
                "No locations yet",
                {listBounds_.x + 8,
                 listBounds_.y + 60,
                 listBounds_.width - 16,
                 24}
            );
        }
        for (std::size_t i = scrollOffset_;
             i < std::min(locations.size(), scrollOffset_ + visible);
             ++i)
        {
            const auto& w = *locations[i];
            const float y = listBounds_.y + 48 + float(i - scrollOffset_) * 74;
            const auto count = map.employment().employed(w.id, citizens);
            const UiRectangle
                row{listBounds_.x + 6, y, listBounds_.width - 12, 66};
            ui.drawButton(renderer, row, "", false, false, false, true);
            hits_.push_back({row, "", w.id});
            fitLabel(
                renderer,
                ui,
                w.name,
                {listBounds_.x + 8, y, listBounds_.width - 16, 25}
            );
            fitLabel(
                renderer,
                ui,
                w.operational
                    ? std::to_string(count) + "/" + std::to_string(w.capacity)
                    : "Under construction",
                {listBounds_.x + 12, y + 30, listBounds_.width - 24, 24}
            );
        }
        button(
            {listBounds_.x + 10, listBounds_.y + height - 38, 30, 26},
            "<",
            {{}, "previous"},
            scrollOffset_ > 0
        );
        button(
            {listBounds_.x + listBounds_.width - 40,
             listBounds_.y + height - 38,
             30,
             26},
            ">",
            {{}, "next"},
            scrollOffset_ + visible < locations.size()
        );
    }
} // namespace Paladin

namespace Paladin
{
    std::string EmploymentPanel::tooltipAt(float x, float y) const
    {
        if (!open_ || dragging_)
        {
            return {};
        }
        for (const auto& hit : hits_)
        {
            if (!hit.bounds.contains(x, y))
            {
                continue;
            }
            if (hit.type == "close")
            {
                return "Close panel";
            }
            if (hit.type == "foundSettlement")
            {
                return "Choose a region for a new settlement";
            }
            if (hit.type == "realmWorkDay")
            {
                return "Change workday in all controlled cities (0-14 hours)";
            }
            if (hit.type == "cityWorkDay")
            {
                return "Change this city's workday (0-14 hours)";
            }
            if (hit.icon)
            {
                if (const auto* d =
                        SettlementObjectCatalog::definition(hit.type))
                {
                    return std::string(d->displayName);
                }
            }
            if (hit.workplace)
            {
                return "Show workplace";
            }
            if (hit.delta)
            {
                return hit.delta > 0 ? "Employ one citizen"
                                     : "Release one worker";
            }
        }
        if (bounds_.contains(x, y))
        {
            return "Drag to move this panel";
        }
        return {};
    }
} // namespace Paladin
