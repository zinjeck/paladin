#pragma once
#include "ui/PanelDrag.h"
#include "ui/UiButton.h"
#include "ui/GrayUiRenderer.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "simulation/WorldShipmentSystem.h"
#include <algorithm>
#include <string>
namespace Paladin
{
    class CaravanPanel
    {
    public:
        void open(ShipmentId id) { close(); selected_ = id; }
        void close() { selected_ = {}; drag_.cancel(); captured_ = false; close_.cancelPress(); stop_.cancelPress(); }
        ShipmentId selection() const noexcept { return selected_; }
        bool isOpen() const noexcept { return bool(selected_); }
        bool contains(float x, float y) const noexcept { return isOpen() && bounds_.contains(x, y); }
        const UiRectangle& bounds() const noexcept { return bounds_; }
        void layout(int width, int height, const World& world, RealmId actor)
        {
            width_ = width; height_ = height;
            const auto* caravan = world.shipment(selected_);
            if (!caravan) { close(); return; }
            const float w = std::min(390.F, std::max(0.F, float(width) - 24));
            const float h = std::min(328.F, std::max(0.F, float(height) - 86));
            bounds_ = drag_.place({float(width) - w - 12, 72, w, h}, width, height);
            close_.setBounds({bounds_.x + w - 42, bounds_.y + 9, 28, 26});
            stop_.setBounds({bounds_.x + 14, bounds_.y + h - 44, w - 28, 30});
            stop_.setEnabled(caravan->owner == actor && caravan->repeating && caravan->active());
        }
        bool handle(const SDL_Event& event, World& world, RealmId actor)
        {
            if (!isOpen()) return false;
            if (!world.shipment(selected_)) { close(); return false; }
            if (drag_.handle(event, bounds_))
            { captured_ = false; close_.cancelPress(); stop_.cancelPress(); layout(width_, height_, world, actor); return true; }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            { captured_ = false; close_.cancelPress(); stop_.cancelPress(); return false; }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
            { close(); return true; }
            if (event.type == SDL_EVENT_MOUSE_MOTION)
            { close_.pointerMoved(event.motion.x, event.motion.y); stop_.pointerMoved(event.motion.x, event.motion.y); return captured_ || contains(event.motion.x, event.motion.y); }
            if (event.type == SDL_EVENT_MOUSE_WHEEL) return contains(event.wheel.mouse_x, event.wheel.mouse_y);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && contains(event.button.x, event.button.y))
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                { captured_ = true; (void)close_.pointerPressed(event.button.x, event.button.y); (void)stop_.pointerPressed(event.button.x, event.button.y); }
                return true;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
            {
                const bool consumed = captured_ || contains(event.button.x, event.button.y); captured_ = false;
                const bool dismiss = close_.pointerReleased(event.button.x, event.button.y);
                if (stop_.pointerReleased(event.button.x, event.button.y))
                    (void)WorldShipmentSystem::stop(world, actor, selected_);
                if (dismiss) close();
                return consumed;
            }
            return false;
        }
        void render(Renderer& renderer, const GrayUiRenderer& ui, const World& world) const
        {
            const auto* caravan = world.shipment(selected_);
            if (!caravan) return;
            ui.drawPanel(renderer, bounds_);
            const auto line = [&](std::string text, float offset, float scale = 1.5F)
            {
                // Long realm/city/resource names must not overflow the inspector.
                const std::size_t limit = std::size_t(std::max(0.F, (bounds_.width - 28) / (6 * scale)));
                if (text.size() > limit && limit > 3) text = text.substr(0, limit - 3) + "...";
                ui.drawLabel(renderer, text, bounds_.x + 14, bounds_.y + offset, scale);
            };
            line("Caravan " + std::to_string(selected_.value()), 16, 2);
            close_.render(renderer, ui);
            const auto* source = world.settlement(caravan->source);
            const auto* destination = world.settlement(caravan->destination);
            const auto* owner = world.realm(caravan->owner);
            const auto* resource = SettlementResourceCatalog::definition(caravan->resource);
            line("Realm: " + std::string(owner ? owner->name() : "Unknown"), 58);
            line("From: " + std::string(source ? source->name() : "Lost settlement"), 87);
            line("To: " + std::string(destination ? destination->name() : "Lost settlement"), 116);
            line("Cargo: " + std::to_string(caravan->cargo) + " " + (resource ? std::string(resource->displayName) : caravan->resource), 145);
            const char* status = caravan->phase == ShipmentPhase::Outbound ? "Delivering" :
                caravan->phase == ShipmentPhase::Returning ? "Returning" :
                caravan->phase == ShipmentPhase::Waiting ? "Waiting for supplies" :
                caravan->phase == ShipmentPhase::Blocked ? "Route blocked; cargo retained" : "Completed";
            line(std::string("Status: ") + status, 174);
            line(std::string("Route: ") + (caravan->repeating ? "Sustained" : "One trip") +
                " | Load: " + std::to_string(caravan->amount), 203);
            line("Deliveries completed: " + std::to_string(caravan->deliveries), 232);
            if (bounds_.height >= 310) stop_.render(renderer, ui);
        }
    private:
        ShipmentId selected_;
        PanelDrag drag_;
        UiRectangle bounds_;
        UiButton close_{"X"}, stop_{"Stop sustained departures"};
        int width_ = 0, height_ = 0;
        bool captured_ = false;
    };
}
