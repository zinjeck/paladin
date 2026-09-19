#pragma once
#include "ui/UiTypes.h"
#include <SDL3/SDL.h>
#include <algorithm>
namespace Paladin
{
    // Shared title-bar capture. It changes presentation coordinates only, never
    // world input, and retains position through layout/resize and reopening.
    class PanelDrag
    {
    public:
        UiRectangle place(UiRectangle initial, int width, int height)
        {
            width_=width; height_=height;
            if (!initialized_) { x_=initial.x; y_=initial.y; initialized_=true; }
            x_=std::clamp(x_,0.F,std::max(0.F,float(width)-initial.width));
            y_=std::clamp(y_,0.F,std::max(0.F,float(height)-initial.height));
            initial.x=x_; initial.y=y_; return initial;
        }
        void cancel() noexcept { dragging_=false; }
        bool active() const noexcept { return dragging_; }
        bool handle(const SDL_Event& e, const UiRectangle& bounds)
        {
            if (e.type==SDL_EVENT_WINDOW_FOCUS_LOST) { cancel(); return false; }
            if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_LEFT &&
                UiRectangle{bounds.x+8,bounds.y+6,std::max(0.F,bounds.width-66),32}.contains(e.button.x,e.button.y))
            { dragging_=true; grabX_=e.button.x-bounds.x; grabY_=e.button.y-bounds.y; return true; }
            if (!dragging_) return false;
            if (e.type==SDL_EVENT_MOUSE_MOTION)
            {
                x_=std::clamp(e.motion.x-grabX_,0.F,std::max(0.F,float(width_)-bounds.width));
                y_=std::clamp(e.motion.y-grabY_,0.F,std::max(0.F,float(height_)-bounds.height)); return true;
            }
            if (e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT)
            { cancel(); return true; }
            return false;
        }
    private:
        bool initialized_=false, dragging_=false;
        float x_=0,y_=0,grabX_=0,grabY_=0;
        int width_=0,height_=0;
    };
}
