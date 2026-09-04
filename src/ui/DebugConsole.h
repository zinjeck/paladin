#pragma once
#include "ui/NormalFontRenderer.h"
#include "ui/UiButton.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
namespace Paladin
{
// Session-wide presentation; commands operate on explicit settlement IDs
// elsewhere.
class DebugConsole
{
  public:
    bool isOpen() const noexcept
    {
        return open_;
    }
    bool wantsKeyboard() const noexcept
    {
        return open_ && focused_;
    }
    bool wantsText() const noexcept
    {
        return wantsKeyboard() && inputFocus_;
    }
    bool statsVisible() const noexcept
    {
        return statsOpen_ && !minimized_;
    }
    void showStats() noexcept
    {
        statsOpen_ = true;
        minimized_ = false;
    }
    void reset();
    void layout(int width, int height);
    bool handle(const SDL_Event& event);
    bool contains(float x, float y) const noexcept;
    std::string takeCommand();
    void print(std::string_view text);
    void render(Renderer& renderer, std::string_view stats);

  private:
    struct Line
    {
        std::size_t begin, end;
    };
    std::vector<Line> wrap(std::string_view text, float width) const;
    std::size_t hitText(float x, float y) const;
    void replaceSelection(std::string text);
    static std::size_t previous(std::string_view text, std::size_t pos);
    static std::size_t next(std::string_view text, std::size_t pos);
    NormalFontRenderer font_;
    bool open_ = false, focused_ = false, inputFocus_ = true, dragging_ = false;
    bool statsOpen_ = false, minimized_ = false;
    UiRectangle panel_, inputBox_, statsBox_;
    std::string input_, output_, command_, draft_;
    std::vector<std::string> history_;
    std::size_t historyIndex_ = 0, cursor_ = 0, anchor_ = 0, inputStart_ = 0;
    std::vector<Line> outputLines_;
    std::vector<Line> statsLines_;
    std::string statsText_;
    float statsTextWidth_ = 0;
    std::size_t firstLine_ = 0;
    int scroll_ = 0, statsScroll_ = 0;
    bool dirty_ = true;
    bool toggleTextPending_ = false;
};
} // namespace Paladin
