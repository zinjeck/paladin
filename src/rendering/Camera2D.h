#pragma once

#include "rendering/PlanetRotation.h"
#include <algorithm>
#include <utility>

namespace Paladin
{
    class Camera2D
    {
    public:
        Camera2D() noexcept = default;

        Camera2D(double tileX, double tileY) noexcept
            : tileX_(tileX), tileY_(tileY)
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

        void setPosition(double tileX, double tileY) noexcept
        {
            tileX_ = tileX;
            tileY_ = tileY;
            planetRotation_.reset();
        }

        void move(double deltaTileX, double deltaTileY) noexcept
        {
            planetRotation_.reset();
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
            zoom_ = std::clamp(zoom, MinimumZoom, MaximumZoom);
        }

        void setWorldZoom(double zoom) noexcept
        {
            zoom_ = std::clamp(zoom, .001, 80.);
        }

        void multiplyZoom(double multiplier) noexcept
        {
            setZoom(zoom_ * multiplier);
        }

        const std::optional<PlanetRotation>& planetRotation() const
        {
            return planetRotation_;
        }
        void setPlanetRotation(PlanetRotation rotation, int width, int height)
        {
            planetRotation_ = rotation.normalized();
            const auto uv = WorldSurface::coordinates(
                planetRotation_->inverse().apply({0, 0, 1})
            );
            tileX_ = uv.u * width;
            tileY_ = uv.v * height;
        }

        [[nodiscard]]
        int cityQuarterTurns() const noexcept
        {
            return cityQuarterTurns_;
        }

        [[nodiscard]]
        double cityHeadingDegrees() const noexcept
        {
            return cityQuarterTurns_ * 90.0;
        }

        void setCityQuarterTurns(int turns) noexcept
        {
            turns %= 4;
            cityQuarterTurns_ = turns < 0 ? turns + 4 : turns;
        }

        void rotateCityQuarterTurns(int delta) noexcept
        {
            setCityQuarterTurns(cityQuarterTurns_ + delta);
        }

        [[nodiscard]]
        std::pair<double, double> cityWorldToViewOffset(
            double x,
            double y
        ) const noexcept
        {
            switch (cityQuarterTurns_)
            {
            case 1:
                return {y, -x};
            case 2:
                return {-x, -y};
            case 3:
                return {-y, x};
            default:
                return {x, y};
            }
        }

        [[nodiscard]]
        std::pair<double, double> cityViewToWorldOffset(
            double x,
            double y
        ) const noexcept
        {
            switch (cityQuarterTurns_)
            {
            case 1:
                return {-y, x};
            case 2:
                return {-x, -y};
            case 3:
                return {y, -x};
            default:
                return {x, y};
            }
        }

    private:
        std::optional<PlanetRotation> planetRotation_;
        static constexpr double MinimumZoom = 0.25;
        static constexpr double MaximumZoom = 80.0;

        // Camera location is expressed in LOGICAL WORLD TILES.
        //
        // It does not know anything about pixels.
        double tileX_ = 0.0;
        double tileY_ = 0.0;

        double zoom_ = 1.0;
        int cityQuarterTurns_ = 0;
    };
} // namespace Paladin
