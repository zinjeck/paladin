#pragma once
#include "core/StrongId.h"
#include "ui/NormalFontRenderer.h"
#include "ui/UiButton.h"
#include <optional>
#include <string>
#include <vector>
namespace Paladin
{
class SettlementMap;
class SettlementCitizenState;
class GrayUiRenderer;
class EmploymentPanel
{
  public:
    void toggle(std::string_view section = "Employment")
    {
        open_ = !open_ || section_ != section;
        section_ = section;
        if (section_ != "Employment")
            selectedType_.clear();
        pressed_ = -1;
        dragging_ = false;
        dragCandidate_ = false;
    }
    void close() noexcept
    {
        open_ = false;
        pressed_ = -1;
        dragging_ = false;
        dragCandidate_ = false;
    }
    bool isOpen() const noexcept
    {
        return open_;
    }
    bool containsPoint(float, float) const noexcept;
    bool pointerPressed(float, float);
    bool pointerMoved(float, float);
    std::string tooltipAt(float, float) const;
    void pointerReleased(
        float,
        float,
        SettlementMap&,
        SettlementCitizenState&,
        double minute
    );
    WorkplaceId takeFocusedWorkplace() noexcept
    {
        const auto id = focusedWorkplace_;
        focusedWorkplace_ = {};
        return id;
    }
    struct WorkDayChange
    {
        bool realm;
        int delta;
    };
    std::optional<WorkDayChange> takeWorkDayChange()
    {
        const auto change = workDayChange_;
        workDayChange_.reset();
        return change;
    }
    void setWorldMode(bool world)
    {
        worldMode_ = world;
    }
    bool takeFoundSettlement()
    {
        const bool result = foundSettlement_;
        foundSettlement_ = false;
        return result;
    }
    void setRealmWorkDayHours(int hours)
    {
        realmWorkDayHours_ = hours;
    }
    void scroll(float amount);
    void render(
        Renderer&,
        const GrayUiRenderer&,
        const SettlementMap&,
        const SettlementCitizenState&,
        double minute
    );

  private:
    bool worldMode_ = false, foundSettlement_ = false;
    bool dragCandidate_ = false;
    float pressX_ = 0, pressY_ = 0;
    bool dragging_ = false;
    bool positioned_ = false;
    float positionX_ = 0, positionY_ = 110;
    float dragX_ = 0, dragY_ = 0;
    float viewportWidth_ = 0, viewportHeight_ = 0;
    int realmWorkDayHours_ = 12;
    std::optional<WorkDayChange> workDayChange_;
    struct Hit
    {
        UiRectangle bounds;
        std::string type;
        WorkplaceId workplace;
        int delta = 0;
        bool icon = false;
    };
    // Other management sections share the window and dismissal behavior.
    std::string section_ = "Employment";
    WorkplaceId focusedWorkplace_;
    bool open_ = false;
    UiRectangle bounds_, listBounds_;
    std::string selectedType_;
    std::vector<Hit> hits_;
    int pressed_ = -1;
    std::size_t scrollOffset_ = 0;
    NormalFontRenderer font_;
};
} // namespace Paladin
