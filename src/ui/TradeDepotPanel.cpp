#include "ui/TradeDepotPanel.h"
#include "simulation/WorldMarketSystem.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <SDL3/SDL.h>
#include <charconv>
namespace Paladin
{
    namespace
    {
        bool hasActiveTradeAgreement(const World& world, RealmId actor)
        {
            return std::any_of(
                world.diplomacy().relations.begin(),
                world.diplomacy().relations.end(),
                [actor](const auto& relation)
                {
                    return (relation.first == actor ||
                            relation.second == actor) &&
                           relation.trading && !relation.atWar;
                }
            );
        }
    } // namespace
    void TradeDepotPanel::open(SettlementId city, SettlementObjectId depot)
    {
        if (city_ == city && depot_ == depot)
        {
            return;
        }
        close();
        city_ = city;
        depot_ = depot;
        resourceIndex_ = 0;
        quantity_ = "10";
        direction_ = TradeDirection::Import;
    }
    void TradeDepotPanel::close() noexcept
    {
        city_ = {};
        depot_ = {};
        controls_.clear();
        message_.clear();
        editing_ = captured_ = false;
        pressed_.reset();
        drag_.cancel();
        quoteSignature_ = ~std::uint64_t(0);
        offer_ = {};
    }
    int TradeDepotPanel::amount() const
    {
        int value = 0;
        const auto result = std::from_chars(
            quantity_.data(),
            quantity_.data() + quantity_.size(),
            value
        );
        return result.ec == std::errc{} ? value : 0;
    }
    std::string_view TradeDepotPanel::resource() const
    {
        const auto definitions = SettlementResourceCatalog::definitions();
        return definitions[resourceIndex_ % definitions.size()].id;
    }
    void TradeDepotPanel::layout(
        int width,
        int height,
        const World& world,
        RealmId actor
    )
    {
        const auto* city = world.settlement(city_);
        const auto* map = city ? city->simulationState().localMap() : nullptr;
        const auto* object =
            map ? map->objectState().completedObject(depot_) : nullptr;
        if (!city || city->ownerRealmId() != actor || !object ||
            object->objectTypeId != SettlementObjectTypes::TradeDepot)
        {
            close();
            return;
        }
        // Revision fingerprinting is cheap and keeps pause-time treaty,
        // inventory and money changes visible without repeatedly forecasting
        // every candidate's economy in the render path.
        std::uint64_t signature = 1469598103934665603ULL;
        const auto mix = [&](std::uint64_t value)
        {
            signature ^= value;
            signature *= 1099511628211ULL;
        };
        mix(world.time().totalGameMinutes());
        mix(world.grid().revision());
        mix(city_.value());
        mix(depot_.value());
        mix(resourceIndex_);
        mix(std::uint64_t(direction_));
        mix(std::uint64_t(amount()));
        for (const auto& relation : world.diplomacy().relations)
        {
            if (relation.first == actor || relation.second == actor)
            {
                mix(relation.first.value());
                mix(relation.second.value());
                mix(relation.trading);
                mix(relation.atWar);
            }
        }
        for (const auto& record : world.settlements())
        {
            mix(record.id().value());
            mix(record.ownerRealmId().value());
            mix(std::uint64_t(record.position().x));
            mix(std::uint64_t(record.position().y));
            mix(record.population());
            const auto& state = record.simulationState();
            mix(state.stockpile().version());
            mix(state.economy().version());
            if (const auto* local = state.localMap())
            {
                mix(local->instanceId());
                mix(local->objectState().navigationVersion());
                mix(local->logistics.version());
            }
        }
        for (const auto& realm : world.realms())
        {
            mix(realm.id().value());
            mix(std::uint64_t(realm.treasury->balance));
        }
        for (const auto partner : map->trade.unreachablePartners)
        {
            mix(partner.value());
        }
        if (signature != quoteSignature_)
        {
            const auto quote = WorldMarketSystem::depotOffer(
                world,
                city_,
                depot_,
                resource(),
                direction_,
                std::max(1, amount())
            );
            offer_ = {
                quote.partner,
                quote.unitPrice,
                quote.available,
                quote.treatyPartners
            };
            quoteSignature_ = signature;
        }
        width_ = width;
        height_ = height;
        const float w = std::min(500.F, std::max(280.F, float(width) - 24));
        const float h = std::min(515.F, std::max(340.F, float(height) - 100));
        bounds_ =
            drag_.place({float(width) - w - 160, 150, w, h}, width, height);
        controls_.clear();
        const float x = bounds_.x + 14, y = bounds_.y, span = w - 28;
        const float verticalScale = h / 515.F;
        auto add =
            [&](float xx, float yy, float ww, Kind kind, std::string label)
        {
            controls_.push_back(
                {{xx, y + (yy - y) * verticalScale, ww, 28 * verticalScale},
                 kind,
                 std::move(label)}
            );
        };
        add(x + span - 28, y + 10, 28, Kind::Close, "X");
        add(x, y + 91, 32, Kind::Previous, "<");
        add(x + span - 32, y + 91, 32, Kind::Next, ">");
        add(x, y + 132, span / 2 - 4, Kind::Import, "Import");
        add(x + span / 2 + 4, y + 132, span / 2 - 4, Kind::Export, "Export");
        add(x, y + 185, 38, Kind::LessTen, "-10");
        add(x + 42, y + 185, 30, Kind::Less, "-");
        add(x + 78, y + 185, span - 156, Kind::Quantity, quantity_);
        add(x + span - 72, y + 185, 30, Kind::More, "+");
        add(x + span - 38, y + 185, 38, Kind::MoreTen, "+10");
        const float bottom = y + 417;
        add(x, bottom, span * .33F - 4, Kind::Dispatch, "One shipment");
        add(x + span * .33F,
            bottom,
            span * .34F - 4,
            Kind::Start,
            "Standing order");
        add(x + span * .67F, bottom, span * .33F, Kind::Stop, "Stop");
    }
    void TradeDepotPanel::act(Kind kind, World& world, RealmId actor)
    {
        quoteSignature_ = ~std::uint64_t(0);
        editing_ = kind == Kind::Quantity;
        const auto count = SettlementResourceCatalog::definitions().size();
        switch (kind)
        {
        case Kind::Close:
            close();
            return;
        case Kind::Previous:
            resourceIndex_ = (resourceIndex_ + count - 1) % count;
            message_.clear();
            break;
        case Kind::Next:
            resourceIndex_ = (resourceIndex_ + 1) % count;
            message_.clear();
            break;
        case Kind::Import:
            direction_ = TradeDirection::Import;
            break;
        case Kind::Export:
            direction_ = TradeDirection::Export;
            break;
        case Kind::Less:
            quantity_ = std::to_string(std::max(1, amount() - 1));
            break;
        case Kind::LessTen:
            quantity_ = std::to_string(std::max(1, amount() - 10));
            break;
        case Kind::More:
            quantity_ = std::to_string(std::min(1000000, amount() + 1));
            break;
        case Kind::MoreTen:
            quantity_ = std::to_string(std::min(1000000, amount() + 10));
            break;
        case Kind::Quantity:
            quantity_.clear();
            break;
        case Kind::Dispatch:
        {
            const auto result = WorldMarketSystem::dispatch(
                world,
                actor,
                city_,
                depot_,
                resource(),
                direction_,
                amount()
            );
            message_ =
                result == ShipmentResult::TradeAgreementRequired &&
                        hasActiveTradeAgreement(world, actor)
                    ? "Trade agreement active; no nearby eligible depots."
                    : shipmentResultText(result);
            break;
        }
        case Kind::Start:
        case Kind::Stop:
            if (WorldMarketSystem::setOrder(
                    world,
                    actor,
                    city_,
                    depot_,
                    resource(),
                    direction_,
                    amount(),
                    kind == Kind::Start
                ))
            {
                message_ =
                    kind == Kind::Start
                        ? "Order saved. Waiting for eligible stock and gold."
                        : "Order stopped; existing cargo is preserved.";
            }
            break;
        }
        if (kind == Kind::Previous || kind == Kind::Next)
        {
            const auto* city = world.settlement(city_);
            const auto* map =
                city ? city->simulationState().localMap() : nullptr;
            if (map)
            {
                for (const auto& order : map->trade.orders)
                {
                    if (order.depot == depot_ && order.resource == resource())
                    {
                        direction_ = order.direction;
                        quantity_ = std::to_string(order.quantity);
                        break;
                    }
                }
            }
        }
        layout(width_, height_, world, actor);
    }
    bool TradeDepotPanel::handle(
        const SDL_Event& event,
        World& world,
        RealmId actor
    )
    {
        if (!isOpen())
        {
            return false;
        }
        if (drag_.handle(event, bounds_))
        {
            captured_ = false;
            pressed_.reset();
            layout(width_, height_, world, actor);
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            close();
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT)
        {
            close();
            return true;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            captured_ = editing_ = false;
            pressed_.reset();
            return false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && editing_)
        {
            int digit = -1;
            if (event.key.scancode >= SDL_SCANCODE_1 &&
                event.key.scancode <= SDL_SCANCODE_9)
            {
                digit = 1 + int(event.key.scancode - SDL_SCANCODE_1);
            }
            else if (
                event.key.scancode == SDL_SCANCODE_0 ||
                event.key.scancode == SDL_SCANCODE_KP_0
            )
            {
                digit = 0;
            }
            else if (
                event.key.scancode >= SDL_SCANCODE_KP_1 &&
                event.key.scancode <= SDL_SCANCODE_KP_9
            )
            {
                digit = 1 + int(event.key.scancode - SDL_SCANCODE_KP_1);
            }
            if (digit >= 0 && quantity_.size() < 7)
            {
                quantity_ += char('0' + digit);
            }
            if (event.key.scancode == SDL_SCANCODE_BACKSPACE &&
                !quantity_.empty())
            {
                quantity_.pop_back();
            }
            if (event.key.scancode == SDL_SCANCODE_RETURN ||
                event.key.scancode == SDL_SCANCODE_KP_ENTER)
            {
                editing_ = false;
            }
            layout(width_, height_, world, actor);
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            mouseX_ = event.motion.x;
            mouseY_ = event.motion.y;
            return captured_ || contains(mouseX_, mouseY_);
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL &&
            contains(event.wheel.mouse_x, event.wheel.mouse_y))
        {
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            !contains(event.button.x, event.button.y))
        {
            editing_ = false;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            contains(event.button.x, event.button.y))
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                captured_ = true;
                pressed_.reset();
                for (const auto& control : controls_)
                {
                    if (control.bounds.contains(event.button.x, event.button.y))
                    {
                        pressed_ = control.kind;
                        break;
                    }
                }
            }
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const bool consumed =
                captured_ || contains(event.button.x, event.button.y);
            const auto pressed = pressed_;
            captured_ = false;
            pressed_.reset();
            if (pressed)
            {
                for (const auto& control : controls_)
                {
                    if (control.kind == *pressed &&
                        control.bounds.contains(event.button.x, event.button.y))
                    {
                        act(*pressed, world, actor);
                        break;
                    }
                }
            }
            return consumed;
        }
        return false;
    }
    void TradeDepotPanel::render(
        Renderer& renderer,
        const GrayUiRenderer& ui,
        const World& world,
        RealmId actor
    ) const
    {
        const auto* city = world.settlement(city_);
        const auto* map = city ? city->simulationState().localMap() : nullptr;
        if (!map || !isOpen())
        {
            return;
        }
        ui.drawPanel(renderer, bounds_);
        const float x = bounds_.x + 14, y = bounds_.y,
                    span = bounds_.width - 28;
        const float verticalScale = bounds_.height / 515.F;
        BitmapFontRenderer font;
        auto text = [&](std::string label, float yy, float size = 1.5F)
        {
            size = std::max(1.F, size * verticalScale);
            yy = y + (yy - y) * verticalScale;
            while (!label.empty() && font.measureWidth(label, size) > span)
            {
                label.pop_back();
            }
            ui.drawLabel(renderer, label, x, yy, size);
        };
        text("WORLD TRADE", y + 15, 2);
        const auto& offer = offer_;
        const bool activeAgreement = hasActiveTradeAgreement(world, actor);
        text(
            offer.treatyPartners ? "Treaty partner cities: " +
                                       std::to_string(offer.treatyPartners)
            : activeAgreement
                ? "Trade agreement active; no nearby eligible depots."
                : "Make a trade agreement in Diplomacy.",
            y + 56,
            activeAgreement && !offer.treatyPartners ? 1.F : 1.5F
        );
        const auto* definition =
            SettlementResourceCatalog::definition(resource());
        const std::string label = definition
                                      ? std::string(definition->displayName)
                                      : std::string(resource());
        ui.drawLabel(
            renderer,
            label,
            x + span / 2 -
                font.measureWidth(label, std::max(1.F, 2 * verticalScale)) / 2,
            y + 96 * verticalScale,
            std::max(1.F, 2 * verticalScale)
        );
        text("Quantity per shipment", y + 167, 1);
        const auto* partner = world.settlement(offer.partner);
        text(
            partner ? "Nearest partner: " + std::string(partner->name())
                    : "No eligible partner stock or demand.",
            y + 233
        );
        text(
            "Price: " + goldText(offer.unitPrice) +
                " gold / unit   Available: " + std::to_string(offer.available),
            y + 259
        );
        text(
            "Order value: " + goldText(offer.unitPrice * amount()) + " gold",
            y + 285
        );
        const auto* exports =
            map->logistics.inventory(map->logistics.forObject(depot_));
        const auto* imports =
            map->logistics.inventory(map->logistics.importsForObject(depot_));
        text(
            "Export stock: " +
                std::to_string(exports ? exports->amount(resource()) : 0) +
                "   Import stock: " +
                std::to_string(imports ? imports->amount(resource()) : 0),
            y + 311
        );
        text(
            "Treasury: " + goldText(map->commerce.treasury->balance) + " gold",
            y + 337
        );
        std::string status = message_;
        for (const auto& order : map->trade.orders)
        {
            if (order.depot == depot_ && order.resource == resource() &&
                order.enabled)
            {
                status = "Active: " + order.status;
            }
        }
        const float bottom = y + 417;
        text(status, bottom + 38, 1);
        text(
            "Imports are sold locally. Exports buy from producers and "
            "stockpiles.",
            bottom + 58,
            1
        );
        text(
            "Nearby deliveries: about 20-60 minutes; distance affects travel.",
            bottom + 73,
            1
        );
        for (const auto& control : controls_)
        {
            if (control.kind == Kind::Quantity)
            {
                ui.drawTextField(
                    renderer,
                    control.bounds,
                    quantity_,
                    "Quantity",
                    editing_
                );
                continue;
            }
            const bool selected = (control.kind == Kind::Import &&
                                   direction_ == TradeDirection::Import) ||
                                  (control.kind == Kind::Export &&
                                   direction_ == TradeDirection::Export);
            ui.drawButton(
                renderer,
                control.bounds,
                control.text,
                control.bounds.contains(mouseX_, mouseY_),
                pressed_ && *pressed_ == control.kind,
                selected,
                true
            );
        }
    }
} // namespace Paladin
