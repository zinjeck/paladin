#pragma once

#include "rendering/Texture.h"
#include "ui/UiButton.h"
#include "world/FoundingIdentity.h"
#include "world/settlements/ResourceFlowHistory.h"
#include <memory>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
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
        Diplomacy,
        FocusActiveSettlement,
        Technology,
        Military,
        Economy,
        Events,
        Ledger,
        ToggleRoofs,
        ToggleResources,
        ToggleEnvironmentArt,
        Back
    };

    class CityHud
    {
    public:
        CityHud();
        void setRealmFlag(const RealmFlag& flag)
        {
            flag_ = flag;
        }
        bool roofControlAt(float x, float y) const
        {
            return artButton_.containsPoint(x, y) ||
                   (!worldMode_ && roofsButton_.containsPoint(x, y));
        }
        void setResourcesVisible(bool value)
        {
            resourceButton_.setSelected(value);
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
        void setWorldMode(bool enabled)
        {
            if (worldMode_ != enabled)
            {
                topButtons_[1].setText(enabled ? "Diplomacy" : "Employment");
            }
            worldMode_ = enabled;
            if (enabled)
            {
                closeCategoryMenus();
            }
        }
        const UiRectangle& activeSettlementBounds() const noexcept
        {
            return activeSettlementPanel_;
        }
        void setActiveSettlementName(std::string name)
        {
            activeSettlementName_ = std::move(name);
            activeSettlementButton_.setText(
                "Active Settlement: " + activeSettlementName_
            );
        }

        void setFortress(bool value) noexcept
        {
            if (fortress_ != value)
            {
                closeCategoryMenus();
                fortress_ = value;
                bottomButtons_[3].setText(value ? "Warfare" : "Agriculture");
            }
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
        void clearGoods() noexcept;
        void setGoodsResource(
            std::string_view resource,
            double amount,
            ResourceDailyRates rates = {}
        ) noexcept;
        // Compatibility helpers for older callers; new scenes use the catalog.
        void setGoodsAmounts(
            double stone,
            double lumber,
            double fish,
            double meat = 0
        ) noexcept;
        void setGoodsDailyRates(
            std::array<ResourceDailyRates, 4> rates
        ) noexcept;
        void renderIdentity(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;
        [[nodiscard]] std::size_t goodsCount() const noexcept
        {
            return goods_.size();
        }
        [[nodiscard]] UiRectangle goodsBounds(
            std::string_view resource
        ) const noexcept;

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
        void reloadArt()
        {
            goodsIconsLoaded_ = false;
            for (auto& icon : goodsIcons_)
            {
                icon.reset();
            }
            treasuryIcon_.reset();
            optionIcons_.clear();
        }

    private:
        bool fortress_ = false;
        std::size_t visibleCategoryCount() const noexcept
        {
            return fortress_ ? 4 : CategoryCount;
        }
        std::size_t displayCategory(std::size_t category) const noexcept
        {
            return !fortress_ || category < 3 ? category
                   : category == 6            ? 3
                                              : CategoryCount;
        }
        bool worldMode_ = false;
        std::string activeSettlementName_;
        UiRectangle activeSettlementPanel_;
        UiButton activeSettlementButton_{"Active Settlement"};
        UiRectangle treasuryPanel_, extensionPanel_;
        std::int64_t treasuryGold_ = 0;

        static constexpr std::size_t CategoryCount = 9;

        [[nodiscard]]
        bool optionIsVisible(std::size_t optionIndex) const noexcept;

        UiButton resourceButton_{"R"};
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
        mutable std::vector<std::shared_ptr<Texture>> goodsIcons_;
        mutable std::shared_ptr<Texture> treasuryIcon_;
        mutable std::unordered_map<std::string, std::shared_ptr<Texture>>
            optionIcons_;
        std::vector<UiRectangle> goodsCells_;
        bool goodsOpen_ = true;
        bool hasKeep_ = false;
        std::size_t population_ = 8;
        std::size_t housingCapacity_ = 0;
        struct GoodsEntry
        {
            std::string_view id;
            std::string_view name;
            double amount = 0;
            ResourceDailyRates rates;
        };
        std::vector<GoodsEntry> goods_;
        std::array<UiButton, CategoryCount> bottomButtons_;
        UiRectangle toolbarBounds_;
        std::vector<UiButton> optionButtons_;
        std::vector<UiRectangle> optionBounds_;
        std::size_t openCategory_ = CategoryCount;
        RealmFlag flag_;
        UiRectangle flagPanel_;
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
