#pragma once

#include "ui/UiButton.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class CityHudAction
    {
        None,
        BeginObjectPlacement,
        BeginCommand,
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

        [[nodiscard]]
        const UiRectangle& minimapBounds() const noexcept
        {
            return minimapPanel_;
        }

        void setGoodsAmounts(double stone, double wood) noexcept
        {
            stoneAmount_ = stone;
            woodAmount_ = wood;
        }

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

        [[nodiscard]]
        std::string_view selectedObjectTypeId() const noexcept;

        [[nodiscard]]
        std::string_view selectedCommandTypeId() const noexcept;

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
        std::array<UiButton, 4> topButtons_;
        UiRectangle minimapPanel_;
        UiButton goodsButton_{"Goods"};
        std::array<UiRectangle, 6> goodsCells_{};
        bool goodsOpen_ = false;
        double stoneAmount_ = 0;
        double woodAmount_ = 0;
        std::array<UiButton, CategoryCount> bottomButtons_;
        std::vector<UiButton> optionButtons_;
        std::vector<UiRectangle> optionBounds_;
        std::size_t openCategory_ = CategoryCount;
        UiRectangle cityNamePanel_;
        UiRectangle dayTimePanel_;
        UiRectangle reservedPanel_;
        std::string cityName_;
        std::string selectedObjectTypeId_;
        std::string selectedCommandTypeId_;
        std::uint64_t day_ = 1;
        int hour_ = 6;
        int minute_ = 0;
    };
}
