#pragma once

namespace Paladin
{
    struct CameraNavigationPolicy
    {
        double keyboardPanSpeedTilesPerSecondAtZoomOne = 162.5;
        double edgePanSpeedTilesPerSecondAtZoomOne = 150.0;
        float edgeActivationWidthPixels = 42.0F;
        double edgeActivationDelaySeconds = 0.10;
        double edgeResponseExponent = 1.65;
    };

    [[nodiscard]]
    const CameraNavigationPolicy&
    defaultCameraNavigationPolicy() noexcept;
}
