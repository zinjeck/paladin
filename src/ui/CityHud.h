#pragma once

#include "ui/UiButton.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class CityHudAction
    {
        None,
        Back
    };

    class CityHud
    {
    public:
        CityHud();

        void layout(
            int viewportWidth,
            int viewportHeight
        ) noexcept;

        void pointerMoved(float x, float y) noexcept;

        void setCityInformation(
            std::string cityName,
            std::uint64_t day,
            int hour,
            int minute
        );

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        bool containsInteractivePoint(float x, float y) const noexcept;

        [[nodiscard]]
        CityHudAction pointerReleased(float x, float y) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;

    private:
        static constexpr std::size_t CategoryCount = 6;

        [[nodiscard]]
        bool optionIsVisible(std::size_t optionIndex) const noexcept;

        void closeCategoryMenus() noexcept;

        UiButton backButton_;
        std::array<UiButton, CategoryCount> bottomButtons_;
        std::vector<UiButton> optionButtons_;
        std::vector<UiRectangle> optionBounds_;
        std::size_t openCategory_ = CategoryCount;
        UiRectangle cityNamePanel_;
        UiRectangle dayTimePanel_;
        UiRectangle reservedPanel_;
        std::string cityName_;
        std::uint64_t day_ = 1;
        int hour_ = 6;
        int minute_ = 0;
    };
}
