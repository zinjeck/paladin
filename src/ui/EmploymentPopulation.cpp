#include "ui/EmploymentPanel.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementMap.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Paladin
{
    std::string EmploymentPanel::tooltipKeyAt(float x, float y) const
    {
        for (std::size_t i = 0; i < attributeBounds_.size(); ++i)
        {
            if (open_ && section_ == "Population" &&
                attributeBounds_[i].contains(x, y))
            {
                return "population-attribute-" + std::to_string(i);
            }
        }
        return tooltipAt(x, y);
    }

    void EmploymentPanel::renderImmigration(
        Renderer& renderer,
        const GrayUiRenderer& ui,
        const SettlementMap& map,
        float top
    )
    {
        const auto available = map.immigration.available();
        admissionCount_ = std::min(admissionCount_, available);
        const auto button = [&](UiRectangle bounds,
                                const std::string& text,
                                std::string type,
                                int delta,
                                bool enabled)
        {
            ui.drawButton(renderer, bounds, text, false, false, false, enabled);
            if (enabled)
            {
                hits_.push_back({bounds, std::move(type), {}, delta});
            }
        };
        const UiRectangle section{
            bounds_.x + 18,
            top + 4,
            bounds_.width - 36,
            bounds_.y + bounds_.height - top - 22
        };
        ui.drawPanel(renderer, section);
        const auto label =
            [&](const std::string& text, float x, float y, float maximumWidth)
        {
            const BitmapFontRenderer font;
            const float scale = std::min(
                2.0F,
                maximumWidth / std::max(1.0F, font.measureWidth(text, 1))
            );
            ui.drawLabel(renderer, text, x, y, scale);
        };
        label(
            "Available Immigrants: " + std::to_string(available),
            section.x + 14,
            section.y + 16,
            section.width - 28
        );
        renderer.drawLine(
            section.x + 12,
            section.y + 45,
            section.x + section.width - 12,
            section.y + 45,
            {110, 110, 115, 255}
        );
        const auto& conditions = map.immigration.conditions();
        std::ostringstream text;
        text << std::fixed << std::setprecision(1)
             << "Stored food: " << conditions.foodDays << " days";
        label(text.str(), section.x + 14, section.y + 63, section.width - 28);
        const float y = section.y + section.height - 48;
        label("Migrate:", section.x + 14, y + 9, 100);
        const bool enabled = !worldMode_ && map.logistics.founded();
        button(
            {section.x + 122, y, 34, 32},
            "<",
            "migrate",
            -1,
            enabled && admissionCount_ > 0
        );
        button(
            {section.x + 162, y, 34, 32},
            ">",
            "migrate",
            1,
            enabled && admissionCount_ < available
        );
        label(
            std::to_string(admissionCount_),
            section.x + 214,
            y + 9,
            std::max(24.0F, section.width - 374)
        );
        button(
            {section.x + section.width - 146, y, 132, 32},
            "Admit",
            "admit",
            0,
            enabled && admissionCount_ > 0
        );
    }
} // namespace Paladin
