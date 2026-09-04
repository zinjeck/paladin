#pragma once

#include "ui/UiTypes.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    class UiTextField
    {
    public:
        UiTextField(
            std::string placeholder,
            std::size_t maximumLength
        );

        void setBounds(UiRectangle bounds) noexcept;
        void setFocused(bool focused) noexcept;

        [[nodiscard]]
        bool focused() const noexcept;

        [[nodiscard]]
        bool contains(float x, float y) const noexcept;

        void appendText(std::string_view text);
        void backspace() noexcept;
        void clear() noexcept;
        void setText(std::string_view text);

        [[nodiscard]]
        const std::string& text() const noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;

    private:
        std::string placeholder_;
        std::string text_;
        std::size_t maximumLength_ = 0;
        UiRectangle bounds_;
        bool focused_ = false;
    };
}
