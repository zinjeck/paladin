#include "ui/UiTextField.h"

#include "ui/GrayUiRenderer.h"

#include <cctype>
#include <utility>

namespace Paladin
{
    UiTextField::UiTextField(
        std::string placeholder,
        std::size_t maximumLength
    )
        : placeholder_(std::move(placeholder)),
          maximumLength_(maximumLength)
    {
    }

    void UiTextField::setBounds(UiRectangle bounds) noexcept
    {
        bounds_ = bounds;
    }

    void UiTextField::setFocused(bool focused) noexcept
    {
        focused_ = focused;
    }

    bool UiTextField::focused() const noexcept
    {
        return focused_;
    }

    bool UiTextField::contains(float x, float y) const noexcept
    {
        return bounds_.contains(x, y);
    }

    void UiTextField::appendText(std::string_view text)
    {
        for (const unsigned char character : text)
        {
            if (text_.size() >= maximumLength_)
            {
                break;
            }

            const bool acceptedCharacter =
                std::isalnum(character) ||
                character == ' ' ||
                character == '-' ||
                character == '\'';

            if (acceptedCharacter)
            {
                text_.push_back(
                    static_cast<char>(character)
                );
            }
        }
    }

    void UiTextField::backspace() noexcept
    {
        if (!text_.empty())
        {
            text_.pop_back();
        }
    }

    void UiTextField::clear() noexcept
    {
        text_.clear();
    }

    void UiTextField::setText(std::string_view text)
    {
        text_.clear();
        appendText(text);
    }

    const std::string& UiTextField::text() const noexcept
    {
        return text_;
    }

    void UiTextField::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    ) const
    {
        uiRenderer.drawTextField(
            renderer,
            bounds_,
            text_,
            placeholder_,
            focused_
        );
    }
}
