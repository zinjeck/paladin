#pragma once

#include "ui/UiButton.h"
#include "rendering/Texture.h"
#include <memory>

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
        Population,
        Laws,
        Employment,
        Technology,
        Military,
        Economy,
        Events,
        Ledger,
        ToggleRoofs,
        ToggleEnvironmentArt,
        Back
    };

    class CityHud
    {
    public:
        CityHud();
        bool roofControlAt(float x, float y) const
        {
            return artButton_.containsPoint(x, y) ||
                   (!worldMode_ && roofsButton_.containsPoint(x, y));
        }
        void setRoofsVisible(bool value)
        {
            roofsButton_.setSelected(value);
        }
        bool reportControlAt(float x, float y) const noexcept
        {
            return eventsButton_.containsPoint(x, y) ||
                   ledgerButton_.containsPoint(x, y);
        }
        void setTreasuryGold(std::int64_t cents) noexcept
        {
            treasuryGold_ = cents;
        }
        void setWorldMode(bool enabled) noexcept
        {
            worldMode_ = enabled;
            if (enabled)
            {
                closeCategoryMenus();
            }
        }
        void setActiveSettlementName(std::string name)
        {
            activeSettlementName_ = std::move(name);
        }

        void closeCategoryMenus() noexcept;

        void layout(int viewportWidth, int viewportHeight) noexcept;

        [[nodiscard]]
        const UiRectangle& minimapBounds() const noexcept
        {
            return minimapPanel_;
        }

        void setSettlementStatus(bool hasKeep, std::size_t population) noexcept;
        void setHousingCapacity(std::size_t capacity) noexcept
        {
            housingCapacity_ = capacity;
        }
        void setGoodsAmounts(
            double stone,
            double lumber,
            double fish,
            double meat = 0
        ) noexcept
        {
            stoneAmount_ = stone;
            lumberAmount_ = lumber;
            fishAmount_ = fish;
            meatAmount_ = meat;
        }

        void pointerMoved(float x, float y) noexcept;
        std::string tooltipAt(float x, float y) const;

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

        void render(Renderer& renderer, const GrayUiRenderer& uiRenderer) const;

    private:
        bool worldMode_ = false;
        std::string activeSettlementName_;
        UiRectangle activeSettlementPanel_;
        UiRectangle treasuryPanel_, extensionPanel_;
        std::int64_t treasuryGold_ = 0;

        static constexpr std::size_t CategoryCount = 6;

        [[nodiscard]]
        bool optionIsVisible(std::size_t optionIndex) const noexcept;

        UiButton roofsButton_{"Roofs O"};
        UiButton artButton_{"Art F8"};
        UiButton backButton_;
        UiButton eventsButton_{"!!!"};
        UiButton ledgerButton_{"L"};
        UiButton populationButton_{""};
        std::array<UiButton, 5> topButtons_;
        UiRectangle minimapPanel_;
        UiButton goodsButton_{"Goods"};
        mutable bool goodsIconsLoaded_ = false;
        mutable std::array<std::shared_ptr<Texture>, 4> goodsIcons_;
        std::array<UiRectangle, 6> goodsCells_{};
        bool goodsOpen_ = true;
        bool hasKeep_ = false;
        std::size_t population_ = 8;
        std::size_t housingCapacity_ = 0;
        double stoneAmount_ = 0;
        double lumberAmount_ = 0;
        double fishAmount_ = 0;
        double meatAmount_ = 0;
        std::array<UiButton, CategoryCount> bottomButtons_;
        UiRectangle toolbarBounds_;
        std::vector<UiButton> optionButtons_;
        std::vector<UiRectangle> optionBounds_;
        std::size_t openCategory_ = CategoryCount;
        UiRectangle cityNamePanel_;
        UiRectangle seasonBounds_;
        UiRectangle dayTimePanel_;
        UiRectangle reservedPanel_;
        std::string cityName_;
        std::string selectedObjectTypeId_;
        std::string selectedCommandTypeId_;
        std::uint64_t day_ = 1;
        int hour_ = 6;
        int minute_ = 0;
    };
} // namespace Paladin
