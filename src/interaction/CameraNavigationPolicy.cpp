#include "interaction/CameraNavigationPolicy.h"

namespace Paladin
{
    const CameraNavigationPolicy&
    defaultCameraNavigationPolicy() noexcept
    {
        static const CameraNavigationPolicy policy;
        return policy;
    }
}
