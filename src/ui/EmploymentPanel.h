#pragma once
#include "core/StrongId.h"
#include "ui/NormalFontRenderer.h"
#include "ui/UiButton.h"
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
    }
    void close() noexcept
    {
        open_ = false;
        pressed_ = -1;
    }
    bool isOpen() const noexcept
    {
        return open_;
    }
    bool containsPoint(float, float) const noexcept;
    bool pointerPressed(float, float);
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
    void scroll(float amount);
    void render(
        Renderer&,
        const GrayUiRenderer&,
        const SettlementMap&,
        const SettlementCitizenState&,
        double minute
    );

  private:
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
