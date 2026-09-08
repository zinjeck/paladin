#pragma once

#include "rendering/PlanetRotation.h"
#include <algorithm>

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
    };
} // namespace Paladin
