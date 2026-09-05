#pragma once
#include "ui/GrayUiRenderer.h"
#include <algorithm>
#include <chrono>
#include <string>

namespace Paladin
{
    // One hover owner at a time; delayed, compact and clamped beside the
    // cursor.
    class UiTooltip
    {
    public:
        void render(
            Renderer& renderer,
            const GrayUiRenderer& ui,
            const std::string& text,
            const std::string& key,
            float x,
            float y
        )
        {
            const auto now = std::chrono::steady_clock::now();
            if (key != key_)
            {
                key_ = key;
                since_ = now;
            }
            if (text.empty() || now - since_ < std::chrono::milliseconds(350))
            {
                return;
            }
            const BitmapFontRenderer font;
            const float scale = std::min(
                1.5F,
                std::max(1.0F, float(renderer.outputWidth()) - 32) /
                    std::max(1.0F, font.measureWidth(text, 1))
            );
            const float width = font.measureWidth(text, scale) + 16;
            const float height = 7 * scale + 16;
            float left = x + 14, top = y + 20;
            if (left + width > renderer.outputWidth() - 8)
            {
                left = x - width - 12;
            }
            if (top + height > renderer.outputHeight() - 8)
            {
                top = y - height - 12;
            }
            left = std::clamp(
                left,
                8.0F,
                std::max(8.0F, renderer.outputWidth() - width - 8)
            );
            top = std::clamp(
                top,
                8.0F,
                std::max(8.0F, renderer.outputHeight() - height - 8)
            );
            ui.drawPanel(renderer, {left, top, width, height});
            ui.drawLabel(renderer, text, left + 8, top + 8, scale);
        }

    private:
        std::string key_;
        std::chrono::steady_clock::time_point since_{};
    };
} // namespace Paladin
