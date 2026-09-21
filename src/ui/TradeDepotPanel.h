#pragma once
#include "core/StrongId.h"
#include "ui/PanelDrag.h"
#include "world/settlements/SettlementTradeState.h"
#include <optional>
#include <string>
#include <vector>
union SDL_Event;
namespace Paladin
{
    class World;
    class Renderer;
    class GrayUiRenderer;
    class TradeDepotPanel
    {
    public:
        void open(SettlementId city, SettlementObjectId depot);
        void close() noexcept;
        bool isOpen() const noexcept
        {
            return bool(depot_);
        }
        SettlementId city() const noexcept
        {
            return city_;
        }
        SettlementObjectId depot() const noexcept
        {
            return depot_;
        }
        bool contains(float x, float y) const noexcept
        {
            return isOpen() && bounds_.contains(x, y);
        }
        bool wantsKeyboard() const noexcept
        {
            return isOpen() && editing_;
        }
        void layout(int width, int height, const World&, RealmId);
        bool handle(const SDL_Event&, World&, RealmId);
        void render(
            Renderer&,
            const GrayUiRenderer&,
            const World&,
            RealmId
        ) const;

    private:
        enum class Kind
        {
            Close,
            Previous,
            Next,
            Import,
            Export,
            Less,
            More,
            LessTen,
            MoreTen,
            Quantity,
            Dispatch,
            Start,
            Stop
        };
        struct Control
        {
            UiRectangle bounds;
            Kind kind;
            std::string text;
        };
        void act(Kind, World&, RealmId);
        int amount() const;
        std::string_view resource() const;
        PanelDrag drag_;
        UiRectangle bounds_;
        SettlementId city_;
        SettlementObjectId depot_;
        int width_ = 0, height_ = 0;
        std::size_t resourceIndex_ = 0;
        TradeDirection direction_ = TradeDirection::Import;
        bool editing_ = false, captured_ = false;
        float mouseX_ = -1, mouseY_ = -1;
        std::string quantity_ = "10", message_;
        std::vector<Control> controls_;
        std::optional<Kind> pressed_;
        struct Quote
        {
            SettlementId partner;
            std::int64_t unitPrice = 0;
            int available = 0;
            std::size_t treatyPartners = 0;
        } offer_;
        std::uint64_t quoteSignature_ = ~std::uint64_t(0);
    };
} // namespace Paladin
