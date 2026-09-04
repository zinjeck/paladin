#pragma once

#include <algorithm>

namespace Paladin
{
    class Camera2D
    {
    public:
        Camera2D() noexcept = default;

        Camera2D(
            double tileX,
            double tileY
        ) noexcept
            : tileX_(tileX),
              tileY_(tileY)
        {
        }

        [[nodiscard]]
        double tileX() const noexcept
        {
            return tileX_;
        }

        [[nodiscard]]
        double tileY() const noexcept
        {
            return tileY_;
        }

        void setPosition(
            double tileX,
            double tileY
        ) noexcept
        {
            tileX_ = tileX;
            tileY_ = tileY;
        }

        void move(
            double deltaTileX,
            double deltaTileY
        ) noexcept
        {
            tileX_ += deltaTileX;
            tileY_ += deltaTileY;
        }

        [[nodiscard]]
        double zoom() const noexcept
        {
            return zoom_;
        }

        void setZoom(double zoom) noexcept
        {
            zoom_ = std::clamp(
                zoom,
                MinimumZoom,
                MaximumZoom
            );
        }

        void multiplyZoom(double multiplier) noexcept
        {
            setZoom(
                zoom_ * multiplier
            );
        }

    private:
        static constexpr double MinimumZoom = 0.25;
        static constexpr double MaximumZoom = 80.0;

        // Camera location is expressed in LOGICAL WORLD TILES.
        //
        // It does not know anything about pixels.
        double tileX_ = 0.0;
        double tileY_ = 0.0;

        double zoom_ = 1.0;
    };
}
