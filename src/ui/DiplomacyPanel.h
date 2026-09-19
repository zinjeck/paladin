#pragma once
#include "ui/PanelDrag.h"
#include "ui/UiTypes.h"
#include "ui/RealmStatistics.h"
#include <optional>
#include <string>
#include <vector>
union SDL_Event;
namespace Paladin
{
    class Renderer;
    class GrayUiRenderer;
    class DiplomacyPanel
    {
    public:
        enum class Sort { Soldiers, Gold, Size, Cities, Fortresses };
        void toggle();
        void open(RealmId realm = {});
        void close() noexcept;
        bool isOpen() const noexcept { return open_; }
        bool contains(float x,float y) const noexcept { return open_ && bounds_.contains(x,y); }
        RealmId selection() const noexcept { return selected_; }
        void layout(int width,int height,const World&,RealmId actor);
        bool handle(const SDL_Event&,World&,RealmId actor);
        void render(Renderer&,const GrayUiRenderer&,const World&,RealmId actor) const;
        const UiRectangle& bounds() const noexcept { return bounds_; }
        std::optional<UiRectangle> sortBounds(Sort sort) const noexcept;
        std::optional<UiRectangle> realmBounds(RealmId id) const noexcept;
        std::optional<UiRectangle> actionBounds(DiplomaticAction action) const noexcept;
    private:
        PanelDrag drag_;
        enum class Kind { Close, Sort, Realm, Action, GiftLess, GiftMore, ScrollUp, ScrollDown, ActionsTab, OpinionsTab, OpinionUp, OpinionDown };
        struct Control { UiRectangle bounds; Kind kind; int value=0; RealmId realm; std::string label; bool enabled=true; };
        void refresh(const World&);
        void act(const Control&,World&,RealmId);
        bool open_=false, captured_=false, dirty_=true, descending_=true;
        RealmId selected_, giftFor_;
        bool opinions_=false; int opinionScroll_=0;
        UiRectangle opinionList_;
        std::vector<RealmId> nearby_;
        Sort sort_=Sort::Soldiers;
        Money gift_=1000;
        int scroll_=0,width_=0,height_=0;
        float mouseX_=-1,mouseY_=-1;
        std::uint64_t statsMinute_=~std::uint64_t{}, areaRevision_=~std::uint64_t{};
        UiRectangle bounds_,list_;
        std::vector<RealmStatistics> stats_;
        std::vector<Control> controls_;
        std::optional<Control> pressed_;
        std::string message_;
    };
}
