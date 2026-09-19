#pragma once
#include "core/StrongId.h"
#include "ui/PanelDrag.h"
#include <optional>
#include <string>
#include <vector>
union SDL_Event;
namespace Paladin
{
    class World;
    class Renderer;
    class GrayUiRenderer;
    // Strategic inspection and dispatch. This is separate from the local-map
    // object inspector and never silently changes the active simulation city.
    class WorldSettlementPanel
    {
    public:
        void open(SettlementId);
        void close() noexcept;
        bool isOpen() const noexcept { return bool(city_); }
        SettlementId selection() const noexcept { return city_; }
        bool choosingDestination() const noexcept { return isOpen() && !resource_.empty(); }
        bool wantsKeyboard() const noexcept { return isOpen() && editing_; }
        bool contains(float x,float y) const noexcept { return isOpen() && bounds_.contains(x,y); }
        const UiRectangle& bounds() const noexcept { return bounds_; }
        void layout(int width,int height,const World&,RealmId);
        bool handle(const SDL_Event&,World&,RealmId);
        bool chooseDestination(World&,RealmId,SettlementId);
        void render(Renderer&,const GrayUiRenderer&,const World&,RealmId) const;
        std::optional<UiRectangle> sendBounds(std::string_view) const;
        std::optional<UiRectangle> destinationBounds(SettlementId) const;
        std::optional<UiRectangle> stopBounds(ShipmentId) const;
    private:
        friend struct ApplicationSmokeTest;
        enum class Kind { Close, Send, Amount, Adjust, All, Once, Repeat, Cancel, Destination, Stop, Up, Down };
        struct Control { UiRectangle bounds; Kind kind; std::string label; int value=0; std::uint64_t id=0; bool enabled=true; };
        void act(const Control&,World&,RealmId);
        int amount() const noexcept;
        PanelDrag drag_;
        SettlementId city_;
        UiRectangle bounds_, list_;
        int width_=0,height_=0,scroll_=0;
        bool repeating_=false,editing_=false,captured_=false;
        float mouseX_=-1,mouseY_=-1;
        std::string resource_, quantity_="1",message_;
        std::vector<Control> controls_;
        std::optional<Control> pressed_;
    };
}
