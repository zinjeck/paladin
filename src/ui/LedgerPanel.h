#pragma once
#include "core/StrongId.h"
#include "simulation/SimulationReports.h"
#include "ui/NormalFontRenderer.h"
#include "ui/UiTypes.h"
#include <limits>
#include <string>
#include <vector>
union SDL_Event;
namespace Paladin
{
    class Simulation;
    class Renderer;
    class GrayUiRenderer;
    struct LedgerCell
    {
        std::string text;
        double number = 0;
        bool numeric = false;
    };
    struct LedgerRow
    {
        std::uint64_t id = 0;
        std::vector<LedgerCell> cells;
    };
    class LedgerPanel
    {
    public:
        void toggle(bool events, bool world, SettlementId city);
        void close()
        {
            open_ = false;
            captured_ = false;
        }
        bool isOpen() const
        {
            return open_;
        }
        bool containsPoint(float x, float y) const
        {
            return open_ && bounds_.contains(x, y);
        }
        void layout(int width, int height);
        bool handle(const SDL_Event&);
        void refresh(const Simulation&);
        void render(Renderer&, const GrayUiRenderer&) const;

    private:
        friend struct ApplicationSmokeTest;
        mutable NormalFontRenderer font_;
        void label(
            Renderer&,
            const GrayUiRenderer&,
            std::string,
            UiRectangle,
            float scale = 1.5F,
            RenderColor = {235, 235, 238, 255}
        ) const;
        void buildRows(const Simulation&);
        void sortRows();
        void changePage(int);
        bool open_ = false, events_ = false, world_ = false, captured_ = false;
        SettlementId city_;
        int page_ = 0, sortColumn_ = 0, offset_ = 0, pressed_ = -1;
        bool descending_ = false;
        std::uint64_t observedVersion_ =
            std::numeric_limits<std::uint64_t>::max();
        UiRectangle bounds_, content_, close_, previous_, next_;
        std::vector<UiRectangle> columns_, tabs_;
        std::vector<std::string> headings_;
        std::vector<LedgerRow> rows_;
        std::deque<ReportSample> samples_;
        std::string title_, subtitle_;
        int visibleRows() const;
    };
} // namespace Paladin
