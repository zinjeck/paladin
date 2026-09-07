#pragma once
#include "ui/GrayUiRenderer.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

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
            std::vector<std::string> lines;
            const auto maximumColumns = std::max<std::size_t>(
                12,
                std::size_t(
                    std::max(72.0F, float(renderer.outputWidth()) - 48) / 9
                )
            );
            std::size_t start = 0;
            while (start < text.size())
            {
                auto end = text.find('\n', start);
                if (end == std::string::npos)
                {
                    end = text.size();
                }
                while (end - start > maximumColumns)
                {
                    auto split = text.rfind(' ', start + maximumColumns);
                    if (split == std::string::npos || split <= start)
                    {
                        split = start + maximumColumns;
                    }
                    lines.push_back(text.substr(start, split - start));
                    start = split + (text[split] == ' ' ? 1 : 0);
                }
                lines.push_back(text.substr(start, end - start));
                start = end + 1;
            }
            float longest = 1;
            for (const auto& line : lines)
            {
                longest = std::max(longest, font.measureWidth(line, 1));
            }
            const float scale = std::min(
                1.5F,
                std::min(
                    std::max(1.0F, float(renderer.outputWidth()) - 32) /
                        longest,
                    std::max(1.0F, float(renderer.outputHeight()) - 32) /
                        std::max(7.0F, float(lines.size()) * 10)
                )
            );
            const float width = longest * scale + 16;
            const float height = float(lines.size()) * 10 * scale + 16;
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
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                ui.drawLabel(
                    renderer,
                    lines[i],
                    left + 8,
                    top + 8 + i * 10 * scale,
                    scale
                );
            }
        }

    private:
        std::string key_;
        std::chrono::steady_clock::time_point since_{};
    };
} // namespace Paladin
