#include "ui/DebugConsole.h"
#include <algorithm>
#include <cmath>
namespace Paladin
{
std::size_t DebugConsole::previous(std::string_view s, std::size_t p)
{
    if (p)
    {
        --p;
    }
    while (p && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
    {
        --p;
    }
    return p;
}
std::size_t DebugConsole::next(std::string_view s, std::size_t p)
{
    if (p < s.size())
    {
        ++p;
    }
    while (p < s.size() && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
    {
        ++p;
    }
    return p;
}
void DebugConsole::reset()
{
    open_ = focused_ = statsOpen_ = minimized_ = dragging_ = false;
    input_.clear();
    output_.clear();
    history_.clear();
    command_.clear();
    cursor_ = anchor_ = inputStart_ = historyIndex_ = 0;
    scroll_ = statsScroll_ = 0;
    dirty_ = true;
}
void DebugConsole::layout(int width, int height)
{
    const float w = std::min(560.f, float(width) * .46f),
                h = std::max(100.f, float(height) - 210);
    if (panel_.width != w)
    {
        dirty_ = true;
    }
    panel_ = {0, 110, w, h};
    inputBox_ = {8, 110 + h - 38, w - 16, 30};
    statsBox_ = {
        float(width) - std::min(650.f, float(width) * .51f),
        110,
        std::min(650.f, float(width) * .51f),
        minimized_ ? 32.f : h
    };
}
bool DebugConsole::contains(float x, float y) const noexcept
{
    return (open_ && panel_.contains(x, y)) ||
           (statsOpen_ && statsBox_.contains(x, y));
}
std::vector<DebugConsole::Line> DebugConsole::wrap(
    std::string_view text,
    float width
) const
{
    std::vector<Line> lines;
    std::size_t start = 0, p = 0;
    while (p < text.size())
    {
        if (text[p] == '\n')
        {
            lines.push_back({start, p});
            start = ++p;
            continue;
        }
        const auto end = next(text, p);
        if (p > start &&
            font_.measureWidth(text.substr(start, end - start)) > width)
        {
            lines.push_back({start, p});
            start = p;
        }
        p = end;
    }
    if (start < text.size())
    {
        lines.push_back({start, text.size()});
    }
    return lines;
}
std::size_t DebugConsole::hitText(float x, float y) const
{
    const std::string& text = inputFocus_ ? input_ : output_;
    std::size_t begin = inputStart_, end = text.size();
    float left = inputBox_.x + 6;
    if (!inputFocus_)
    {
        if (outputLines_.empty())
        {
            return 0;
        }
        auto row = std::clamp(
            int((y - panel_.y - 8) / 24) + int(firstLine_),
            0,
            int(outputLines_.size()) - 1
        );
        begin = outputLines_[row].begin;
        end = outputLines_[row].end;
        left = panel_.x + 10;
    }
    auto p = begin;
    while (p < end)
    {
        auto q = next(text, p);
        float a =
            font_.measureWidth(std::string_view(text).substr(begin, p - begin));
        float b =
            font_.measureWidth(std::string_view(text).substr(begin, q - begin));
        if (x < left + (a + b) / 2)
        {
            break;
        }
        p = q;
    }
    return p;
}
void DebugConsole::replaceSelection(std::string text)
{
    text.erase(
        std::remove_if(
            text.begin(),
            text.end(),
            [](unsigned char c) { return c < 32 || c == 127; }
        ),
        text.end()
    );
    auto a = std::min(cursor_, anchor_), b = std::max(cursor_, anchor_);
    if (input_.size() - (b - a) + text.size() > 4096)
    {
        return;
    }
    input_.replace(a, b - a, text);
    cursor_ = anchor_ = a + text.size();
}
void DebugConsole::print(std::string_view text)
{
    output_.append(text);
    output_ += '\n';
    if (output_.size() > 65536)
    {
        auto cut = output_.find('\n', output_.size() - 65536);
        if (cut != std::string::npos)
        {
            output_.erase(0, cut + 1);
        }
    }
    if (!inputFocus_)
    {
        cursor_ = anchor_ = 0;
    }
    dirty_ = true;
    scroll_ = 0;
}
std::string DebugConsole::takeCommand()
{
    auto result = std::move(command_);
    command_.clear();
    return result;
}
bool DebugConsole::handle(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_TEXT_INPUT && toggleTextPending_)
    {
        toggleTextPending_ = false;
        if (std::string_view(e.text.text) == "`" ||
            std::string_view(e.text.text) == "~")
        {
            return true;
        }
    }
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_GRAVE)
    {
        if (!e.key.repeat)
        {
            toggleTextPending_ = true;
            open_ = !open_;
            focused_ = open_;
            inputFocus_ = true;
            cursor_ = anchor_ = input_.size();
            dragging_ = false;
        }
        return true;
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL)
    {
        if (statsOpen_ && statsBox_.contains(e.wheel.mouse_x, e.wheel.mouse_y))
        {
            statsScroll_ = std::max(0, statsScroll_ + int(-e.wheel.y * 3));
            return true;
        }
        if (open_ && panel_.contains(e.wheel.mouse_x, e.wheel.mouse_y))
        {
            scroll_ = std::max(0, scroll_ + int(e.wheel.y * 3));
            return true;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (statsOpen_ && statsBox_.contains(e.button.x, e.button.y))
        {
            if (e.button.button == SDL_BUTTON_LEFT &&
                e.button.y < statsBox_.y + 30)
            {
                if (e.button.x < statsBox_.x + 32)
                {
                    minimized_ = !minimized_;
                }
                else if (e.button.x < statsBox_.x + 64)
                {
                    statsOpen_ = false;
                }
            }
            return true;
        }
        if (open_ && panel_.contains(e.button.x, e.button.y))
        {
            if (e.button.button == SDL_BUTTON_LEFT)
            {
                focused_ = true;
                inputFocus_ = inputBox_.contains(e.button.x, e.button.y);
                cursor_ = anchor_ = hitText(e.button.x, e.button.y);
                dragging_ = true;
            }
            return true;
        }
        focused_ = false;
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && dragging_)
    {
        cursor_ = hitText(e.motion.x, e.motion.y);
        return true;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && dragging_)
    {
        dragging_ = false;
        return true;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && contains(e.button.x, e.button.y))
    {
        return true;
    }
    if (!wantsKeyboard())
    {
        return false;
    }
    if (e.type == SDL_EVENT_TEXT_INPUT)
    {
        if (inputFocus_)
        {
            replaceSelection(e.text.text);
        }
        return true;
    }
    if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP)
    {
        return false;
    }
    if (e.type == SDL_EVENT_KEY_UP)
    {
        return true;
    }
    const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0,
               shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
    auto& text = inputFocus_ ? input_ : output_;
    if (ctrl && e.key.scancode == SDL_SCANCODE_A)
    {
        anchor_ = 0;
        cursor_ = text.size();
        return true;
    }
    if (ctrl &&
        (e.key.scancode == SDL_SCANCODE_C || e.key.scancode == SDL_SCANCODE_X))
    {
        if (cursor_ != anchor_)
        {
            auto s = text.substr(
                std::min(cursor_, anchor_),
                std::max(cursor_, anchor_) - std::min(cursor_, anchor_)
            );
            SDL_SetClipboardText(s.c_str());
            if (inputFocus_ && e.key.scancode == SDL_SCANCODE_X)
            {
                replaceSelection("");
            }
        }
        return true;
    }
    if (!inputFocus_)
    {
        return true;
    }
    if (ctrl && e.key.scancode == SDL_SCANCODE_V)
    {
        char* s = SDL_GetClipboardText();
        if (s)
        {
            replaceSelection(s);
            SDL_free(s);
        }
        return true;
    }
    switch (e.key.scancode)
    {
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        if (!input_.empty())
        {
            command_ = input_;
            print("> " + input_);
            history_.push_back(input_);
            if (history_.size() > 128)
            {
                history_.erase(history_.begin());
            }
            historyIndex_ = history_.size();
            input_.clear();
            cursor_ = anchor_ = inputStart_ = 0;
        }
        break;
    case SDL_SCANCODE_BACKSPACE:
        if (cursor_ == anchor_)
        {
            anchor_ = previous(text, cursor_);
        }
        replaceSelection("");
        break;
    case SDL_SCANCODE_DELETE:
        if (cursor_ == anchor_)
        {
            anchor_ = next(text, cursor_);
        }
        replaceSelection("");
        break;
    case SDL_SCANCODE_LEFT:
        cursor_ = previous(text, cursor_);
        if (!shift)
        {
            anchor_ = cursor_;
        }
        break;
    case SDL_SCANCODE_RIGHT:
        cursor_ = next(text, cursor_);
        if (!shift)
        {
            anchor_ = cursor_;
        }
        break;
    case SDL_SCANCODE_HOME:
        cursor_ = 0;
        if (!shift)
        {
            anchor_ = cursor_;
        }
        break;
    case SDL_SCANCODE_END:
        cursor_ = text.size();
        if (!shift)
        {
            anchor_ = cursor_;
        }
        break;
    case SDL_SCANCODE_UP:
        if (historyIndex_ == history_.size())
        {
            draft_ = input_;
        }
        if (historyIndex_)
        {
            input_ = history_[--historyIndex_];
        }
        cursor_ = anchor_ = input_.size();
        break;
    case SDL_SCANCODE_DOWN:
        if (historyIndex_ < history_.size())
        {
            ++historyIndex_;
        }
        input_ =
            historyIndex_ < history_.size() ? history_[historyIndex_] : draft_;
        cursor_ = anchor_ = input_.size();
        break;
    default:
        break;
    }
    return true;
}
void DebugConsole::render(Renderer& r, std::string_view stats)
{
    const auto box = [&](UiRectangle b, RenderColor c)
    { r.fillRectangle(b.x, b.y, b.width, b.height, c); };
    if (open_)
    {
        box(panel_, {145, 147, 152, 255});
        box({panel_.x + 2, panel_.y + 2, panel_.width - 4, panel_.height - 4},
            {63, 64, 69, 255});
        if (dirty_)
        {
            outputLines_ = wrap(output_, panel_.width - 20);
            dirty_ = false;
        }
        const int visible = std::max(1, int((panel_.height - 54) / 24));
        scroll_ = std::clamp(
            scroll_,
            0,
            std::max(0, int(outputLines_.size()) - visible)
        );
        firstLine_ = std::max(0, int(outputLines_.size()) - visible - scroll_);
        for (std::size_t i = firstLine_;
             i < outputLines_.size() && i < firstLine_ + visible;
             ++i)
        {
            auto line = outputLines_[i];
            float y = panel_.y + 8 + float(i - firstLine_) * 24;
            if (!inputFocus_)
            {
                auto a = std::max(line.begin, std::min(cursor_, anchor_)),
                     b = std::min(line.end, std::max(cursor_, anchor_));
                if (b > a)
                {
                    box({panel_.x + 10 +
                             font_.measureWidth(
                                 std::string_view(output_)
                                     .substr(line.begin, a - line.begin)
                             ),
                         y,
                         font_.measureWidth(
                             std::string_view(output_).substr(a, b - a)
                         ),
                         23},
                        {100, 110, 129, 255});
                }
            }
            font_.drawText(
                r,
                std::string_view(output_)
                    .substr(line.begin, line.end - line.begin),
                panel_.x + 10,
                y
            );
        }
        box(inputBox_, {27, 28, 32, 255});
        if (inputFocus_)
        {
            inputStart_ = std::min(inputStart_, cursor_);
            while (inputStart_ < cursor_ &&
                   font_.measureWidth(
                       std::string_view(input_)
                           .substr(inputStart_, cursor_ - inputStart_)
                   ) > inputBox_.width - 18)
            {
                inputStart_ = next(input_, inputStart_);
            }
        }
        inputStart_ = std::min(inputStart_, input_.size());
        auto end = inputStart_;
        while (end < input_.size())
        {
            auto q = next(input_, end);
            if (font_.measureWidth(
                    std::string_view(input_)
                        .substr(inputStart_, q - inputStart_)
                ) > inputBox_.width - 12)
            {
                break;
            }
            end = q;
        }
        auto a = std::max(inputStart_, std::min(cursor_, anchor_)),
             b = std::min(end, std::max(cursor_, anchor_));
        if (inputFocus_ && b > a)
        {
            box({inputBox_.x + 6 +
                     font_.measureWidth(
                         std::string_view(input_)
                             .substr(inputStart_, a - inputStart_)
                     ),
                 inputBox_.y + 3,
                 font_.measureWidth(std::string_view(input_).substr(a, b - a)),
                 24},
                {100, 110, 129, 255});
        }
        font_.drawText(
            r,
            std::string_view(input_).substr(inputStart_, end - inputStart_),
            inputBox_.x + 6,
            inputBox_.y + 4
        );
        if (wantsText() && SDL_GetTicks() % 1000 < 550)
        {
            float x = inputBox_.x + 6 +
                      font_.measureWidth(
                          std::string_view(input_)
                              .substr(inputStart_, cursor_ - inputStart_)
                      );
            box({x, inputBox_.y + 4, 1, 22}, {245, 245, 245, 255});
        }
    }
    if (statsOpen_)
    {
        // Deliberate visual exception: only diagnostic stats use translucent
        // black/blue.
        box(statsBox_, {0, 0, 0, 174});
        r.drawLine(
            statsBox_.x,
            statsBox_.y,
            statsBox_.x + statsBox_.width,
            statsBox_.y,
            {30, 145, 230, 180}
        );
        box({statsBox_.x + 2, statsBox_.y + 2, 28, 26}, {10, 40, 65, 220});
        box({statsBox_.x + 34, statsBox_.y + 2, 28, 26}, {10, 40, 65, 220});
        font_.drawText(
            r,
            minimized_ ? "+" : "-",
            statsBox_.x + 9,
            statsBox_.y + 4
        );
        font_.drawText(r, "x", statsBox_.x + 41, statsBox_.y + 4);
        font_.drawText(r, "Debug stats", statsBox_.x + 76, statsBox_.y + 4);
        if (!minimized_)
        {
            if (statsText_ != stats || statsTextWidth_ != statsBox_.width)
            {
                statsText_ = stats;
                statsTextWidth_ = statsBox_.width;
                statsLines_ = wrap(statsText_, statsBox_.width - 20);
            }
            const auto& lines = statsLines_;
            int visible = std::max(1, int((statsBox_.height - 40) / 24));
            statsScroll_ = std::clamp(
                statsScroll_,
                0,
                std::max(0, int(lines.size()) - visible)
            );
            for (int i = statsScroll_;
                 i < int(lines.size()) && i < statsScroll_ + visible;
                 ++i)
            {
                font_.drawText(
                    r,
                    stats.substr(lines[i].begin, lines[i].end - lines[i].begin),
                    statsBox_.x + 10,
                    statsBox_.y + 36 + (i - statsScroll_) * 24,
                    {209, 240, 255, 255}
                );
            }
        }
    }
}
} // namespace Paladin
