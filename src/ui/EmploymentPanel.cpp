#include "ui/EmploymentPanel.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace Paladin
{
namespace
{
void icon(Renderer& renderer, const UiRectangle& box, std::string_view type)
{
    const auto* d = SettlementObjectCatalog::definition(type);
    if (!d)
        return;
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
    renderer.fillRectangle(x, y, w, h, {edge[0], edge[1], edge[2], 255});
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
} // namespace
bool EmploymentPanel::containsPoint(float x, float y) const noexcept
{
    return open_ && (bounds_.contains(x, y) ||
                     (!selectedType_.empty() && listBounds_.contains(x, y)));
}
bool EmploymentPanel::pointerPressed(float x, float y)
{
    pressed_ = -1;
    if (!containsPoint(x, y))
        return false;
    for (std::size_t i = 0; i < hits_.size(); ++i)
        if (hits_[i].bounds.contains(x, y))
        {
            pressed_ = int(i);
            break;
        }
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
    if (!open_ || pressed_ < 0 || std::size_t(pressed_) >= hits_.size())
    {
        pressed_ = -1;
        return;
    }
    const auto hit = hits_[pressed_];
    pressed_ = -1;
    if (!hit.bounds.contains(x, y))
        return;
    if (hit.type == "close")
    {
        close();
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
        map.employment().adjust(hit.workplace, hit.delta, citizens);
    else
        map.employment().adjustType(hit.type, hit.delta, citizens);
    map.employment().record(minute, citizens);
}
void EmploymentPanel::scroll(float amount)
{
    if (!selectedType_.empty())
        scrollOffset_ = amount > 0 ? (scrollOffset_ > 0 ? scrollOffset_ - 1 : 0)
                                   : scrollOffset_ + 1;
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
        return;
    hits_.clear();
    const float width = std::min(730.0F, float(renderer.outputWidth()) * .62F);
    const float height = std::min(540.0F, float(renderer.outputHeight()) - 150);
    const float left =
        std::max(8.0F, (float(renderer.outputWidth()) - width - 292) * .5F);
    bounds_ = {left, 110, width, height};
    listBounds_ = {
        left + width + 8,
        110,
        std::min(284.0F, float(renderer.outputWidth()) - left - width - 16),
        height
    };
    ui.drawPanel(renderer, bounds_);
    const auto button =
        [&](UiRectangle b, std::string_view text, Hit hit, bool enabled = true)
    {
        ui.drawButton(renderer, b, text, false, false, false, enabled);
        if (enabled)
        {
            hit.bounds = b;
            hits_.push_back(std::move(hit));
        }
    };
    ui.drawLabel(renderer, section_, left + 18, 126, 2.4F);
    button({left + width - 38, 116, 30, 28}, "X", {{}, "close"});
    if (section_ != "Employment")
        return;
    const auto& history = map.employment().history();
    const double percent = citizens.citizens().empty()
                               ? 0
                               : 100.0 * map.employment().unemployed(citizens) /
                                     citizens.citizens().size();
    font_.drawText(
        renderer,
        "Unemployed: " + std::to_string(map.employment().unemployed(citizens)) +
            " / " + std::to_string(citizens.citizens().size()) + "  (" +
            percentage(percent) + ")",
        left + 18,
        157
    );
    const UiRectangle graph{
        left + 58,
        208,
        width - 82,
        std::clamp(height - 350.0F, 90.0F, 200.0F)
    };
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
            ui.drawLabel(
                renderer,
                std::to_string(100 - i * 10) + "%",
                graph.x - 46,
                y - 6,
                2.0F,
                {201, 204, 211, 255}
            );
    }
    for (int i = 0; i <= 15; ++i)
    {
        const float x = graph.x + graph.width * i / 15;
        renderer
            .drawLine(x, graph.y, x, graph.y + graph.height, {61, 66, 74, 255});
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
    const auto point = [&](const UnemploymentSample& sample)
    {
        return std::pair{
            graph.x + float(std::clamp(
                          (sample.gameMinute / 1440 + 1 - startDay) / 15,
                          0.0,
                          1.0
                      )) * graph.width,
            graph.y + float(1 - sample.unemployedPercent / 100) * graph.height
        };
    };
    for (std::size_t i = 1; i < history.size(); ++i)
    {
        if (history[i].gameMinute / 1440 + 1 < startDay)
            continue;
        const auto a = point(history[i - 1]), b = point(history[i]);
        renderer
            .drawLine(a.first, a.second, b.first, a.second, {240, 65, 72, 255});
        renderer.drawLine(
            a.first,
            a.second + 1,
            b.first,
            a.second + 1,
            {240, 65, 72, 255}
        );
        renderer
            .drawLine(b.first, a.second, b.first, b.second, {240, 65, 72, 255});
    }
    if (!history.empty())
    {
        const auto a = point(history.back()), b = point({minute, percent});
        renderer
            .drawLine(a.first, a.second, b.first, b.second, {240, 65, 72, 255});
        renderer.fillRectangle(
            b.first - 2,
            b.second - 2,
            4,
            4,
            {255, 107, 111, 255}
        );
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
        ui.drawButton(renderer, card, "", false, false, selectedType_ == d.objectTypeId, true);
        const UiRectangle iconBox{card.x + 8, card.y + 5, 43, 40};
        if (selectedType_ == d.objectTypeId)
            renderer.fillRectangle(
                iconBox.x,
                iconBox.y,
                iconBox.width,
                iconBox.height,
                {144, 149, 158, 255}
            );
        icon(renderer, iconBox, d.objectTypeId);
        std::size_t count = 0, capacity = 0, maximumCapacity = 0;
        for (const auto& w : map.employment().workplaces())
            if (w.objectTypeId == d.objectTypeId)
            {
                count += map.employment().employed(w.id, citizens);
                capacity += w.capacity;
                if (w.operational)
                    maximumCapacity += w.maximumCapacity;
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
        return;
    ui.drawPanel(renderer, listBounds_);
    const auto* d = SettlementObjectCatalog::definition(selectedType_);
    fitLabel(
        renderer,
        ui,
        d ? d->displayName : selectedType_,
        {listBounds_.x + 8, listBounds_.y + 9, listBounds_.width - 16, 30}
    );
    std::vector<const Workplace*> locations;
    for (const auto& w : map.employment().workplaces())
        if (w.objectTypeId == selectedType_)
            locations.push_back(&w);
    const std::size_t visible = std::max(1, int((height - 105) / 74));
    scrollOffset_ = std::min(
        scrollOffset_,
        locations.size() > visible ? locations.size() - visible : 0
    );
    if (locations.empty())
        fitLabel(
            renderer,
            ui,
            "No locations yet",
            {listBounds_.x + 8, listBounds_.y + 60, listBounds_.width - 16, 24}
        );
    for (std::size_t i = scrollOffset_;
         i < std::min(locations.size(), scrollOffset_ + visible);
         ++i)
    {
        const auto& w = *locations[i];
        const float y = listBounds_.y + 48 + float(i - scrollOffset_) * 74;
        const auto count = map.employment().employed(w.id, citizens);
        const UiRectangle row{listBounds_.x + 6, y, listBounds_.width - 12, 66};
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
