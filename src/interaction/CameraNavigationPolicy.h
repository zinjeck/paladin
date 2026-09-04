#pragma once

namespace Paladin
{
    struct CameraNavigationPolicy
    {
        double keyboardPanSpeedScreenPixelsPerSecond = 650.0;
        double edgePanSpeedScreenPixelsPerSecond = 650.0;
        float edgeActivationWidthPixels = 42.0F;
        double edgeActivationDelaySeconds = 0.10;
        double edgeResponseExponent = 1.65;
    };

    [[nodiscard]]
    const CameraNavigationPolicy&
    defaultCameraNavigationPolicy() noexcept;
}
