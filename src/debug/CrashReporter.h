#pragma once

namespace Paladin
{
    class CrashReporter
    {
    public:
        // Install before SDL or game state initialization. Reports remain on
        // this computer; there is no network submission.
        static void install() noexcept;
        static void reportFatal(const char* message) noexcept;
        [[nodiscard]] static const wchar_t* directory() noexcept;
    };
} // namespace Paladin
