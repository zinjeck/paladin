#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace
{
    using namespace Gdiplus;

    bool ensureGdiplus()
    {
        static ULONG_PTR token = 0;
        static bool started = false;
        if (!started)
        {
            GdiplusStartupInput input;
            started = GdiplusStartup(&token, &input, nullptr) == Ok;
        }
        return started;
    }

    int pngEncoderClsid(CLSID& clsid)
    {
        UINT count = 0, bytes = 0;
        GetImageEncodersSize(&count, &bytes);
        if (!count || !bytes)
            return 1;

        std::vector<unsigned char> storage(bytes);
        auto* encoders = reinterpret_cast<ImageCodecInfo*>(storage.data());
        if (GetImageEncoders(count, bytes, encoders) != Ok)
            return 2;

        for (UINT i = 0; i < count; ++i)
        {
            if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
            {
                clsid = encoders[i].Clsid;
                return 0;
            }
        }
        return 3;
    }

    constexpr wchar_t DesktopName[] = L"PaladinAutomation";
    constexpr wchar_t DesktopPath[] = L"winsta0\\PaladinAutomation";

    HDESK openOrCreateDesktop()
    {
        HDESK desktop = OpenDesktopW(
            DesktopName,
            0,
            FALSE,
            DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE |
                DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
                DESKTOP_JOURNALRECORD | DESKTOP_READOBJECTS |
                DESKTOP_SWITCHDESKTOP | DESKTOP_WRITEOBJECTS |
                GENERIC_READ | GENERIC_WRITE
        );
        if (desktop)
            return desktop;

        desktop = CreateDesktopW(
            DesktopName,
            nullptr,
            nullptr,
            0,
            DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE |
                DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
                DESKTOP_JOURNALRECORD | DESKTOP_READOBJECTS |
                DESKTOP_SWITCHDESKTOP | DESKTOP_WRITEOBJECTS |
                GENERIC_READ | GENERIC_WRITE,
            nullptr
        );
        return desktop;
    }

    struct WindowSearch
    {
        HWND hwnd = nullptr;
        DWORD pid = 0;
    };

    BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM param)
    {
        auto* result = reinterpret_cast<WindowSearch*>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid)
            return TRUE;

        wchar_t title[256]{};
        GetWindowTextW(hwnd, title, 256);
        if (std::wstring(title) == L"Paladin")
        {
            result->hwnd = hwnd;
            result->pid = pid;
            return FALSE;
        }
        return TRUE;
    }

    WindowSearch findPaladin(HDESK desktop, int timeoutMs = 10000)
    {
        const auto deadline = GetTickCount64() + static_cast<ULONGLONG>(timeoutMs);
        do
        {
            WindowSearch result;
            EnumDesktopWindows(
                desktop,
                enumWindowsProc,
                reinterpret_cast<LPARAM>(&result)
            );
            if (result.hwnd)
                return result;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (GetTickCount64() < deadline);
        return {};
    }

    std::filesystem::path rootPath()
    {
        wchar_t module[MAX_PATH]{};
        GetModuleFileNameW(nullptr, module, MAX_PATH);
        auto path = std::filesystem::path(module).parent_path();
        return path.parent_path().parent_path();
    }

    LPARAM packPoint(int x, int y)
    {
        return MAKELPARAM(static_cast<short>(x), static_cast<short>(y));
    }

    bool postMouse(HWND hwnd, UINT msg, WPARAM state, int x, int y)
    {
        return PostMessageW(hwnd, msg, state, packPoint(x, y)) != FALSE;
    }

    bool parseInt(const wchar_t* text, int& out)
    {
        try
        {
            out = std::stoi(text);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseDouble(const wchar_t* text, double& out)
    {
        try
        {
            out = std::stod(text);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    int virtualKey(const std::wstring& key)
    {
        std::wstring upper = key;
        std::transform(
            upper.begin(),
            upper.end(),
            upper.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); }
        );

        if (upper == L"ENTER") return VK_RETURN;
        if (upper == L"ESC" || upper == L"ESCAPE") return VK_ESCAPE;
        if (upper == L"TAB") return VK_TAB;
        if (upper == L"SPACE") return VK_SPACE;
        if (upper == L"LEFT") return VK_LEFT;
        if (upper == L"RIGHT") return VK_RIGHT;
        if (upper == L"UP") return VK_UP;
        if (upper == L"DOWN") return VK_DOWN;
        if (upper == L"HOME") return VK_HOME;
        if (upper == L"END") return VK_END;
        if (upper == L"PGUP") return VK_PRIOR;
        if (upper == L"PGDN") return VK_NEXT;
        if (upper.size() >= 2 && upper[0] == L'F')
        {
            const int n = _wtoi(upper.c_str() + 1);
            if (n >= 1 && n <= 12)
                return VK_F1 + (n - 1);
        }
        if (upper.size() == 1)
        {
            const SHORT vk = VkKeyScanW(upper[0]);
            if (vk != -1)
                return LOBYTE(vk);
        }
        return 0;
    }

    LPARAM keyLParam(int vk, bool up)
    {
        const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LPARAM value = 1 | (static_cast<LPARAM>(scan & 0xff) << 16);
        if (up)
            value |= (1LL << 30) | (1LL << 31);
        return value;
    }

    void postKey(HWND hwnd, int vk, bool up)
    {
        PostMessageW(hwnd, up ? WM_KEYUP : WM_KEYDOWN, vk, keyLParam(vk, up));
    }

    bool ensureDesktopThread(HDESK desktop)
    {
        return SetThreadDesktop(desktop) != FALSE;
    }

    int launch(HDESK desktop, bool restart)
    {
        auto existing = findPaladin(desktop, 200);
        if (existing.hwnd && restart)
        {
            PostMessageW(existing.hwnd, WM_CLOSE, 0, 0);
            for (int i = 0; i < 30; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!IsWindow(existing.hwnd))
                    break;
            }
            if (IsWindow(existing.hwnd))
            {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, existing.pid);
                if (process)
                {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
            existing = {};
        }

        if (!existing.hwnd)
        {
            const auto root = rootPath();
            const auto exe = root / L"out" / L"build" / L"x64-Debug" / L"Paladin.exe";
            if (!std::filesystem::exists(exe))
            {
                std::wcerr << L"Paladin.exe not found at " << exe.wstring() << L"\n";
                return 2;
            }

            std::wstring command = L"\"" + exe.wstring() + L"\"";
            std::vector<wchar_t> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back(L'\0');

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.lpDesktop = const_cast<wchar_t*>(DesktopPath);
            PROCESS_INFORMATION process{};

            const std::wstring workdir = exe.parent_path().wstring();
            if (!CreateProcessW(
                    exe.c_str(),
                    mutableCommand.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    CREATE_NEW_PROCESS_GROUP,
                    nullptr,
                    workdir.c_str(),
                    &startup,
                    &process))
            {
                std::wcerr << L"CreateProcess failed: " << GetLastError() << L"\n";
                return 3;
            }
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }

        auto window = findPaladin(desktop, 10000);
        if (!window.hwnd)
        {
            std::wcerr << L"Paladin window did not appear on automation desktop.\n";
            return 4;
        }

        SetWindowPos(
            window.hwnd,
            nullptr,
            40,
            40,
            1280,
            800,
            SWP_NOACTIVATE | SWP_NOZORDER
        );

        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }

        RECT rect{};
        GetClientRect(window.hwnd, &rect);
        std::wcout << L"launched pid=" << window.pid << L" client="
                   << (rect.right - rect.left) << L"x"
                   << (rect.bottom - rect.top)
                   << L" desktop=" << DesktopName << L"\n";
        return 0;
    }

    int captureWindow(HWND hwnd, const std::filesystem::path& output)
    {
        RECT rect{};
        if (!GetClientRect(hwnd, &rect))
            return 1;

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0)
            return 2;

        HDC windowDc = GetDC(hwnd);
        HDC memoryDc = CreateCompatibleDC(windowDc);
        HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
        HGDIOBJ old = SelectObject(memoryDc, bitmap);

        const BOOL printed = PrintWindow(hwnd, memoryDc, PW_RENDERFULLCONTENT);

        if (!printed)
        {
            SelectObject(memoryDc, old);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            ReleaseDC(hwnd, windowDc);
            return 3;
        }

        std::filesystem::create_directories(output.parent_path());

        int result = 0;
        if (!ensureGdiplus())
        {
            result = 4;
        }
        else
        {
            CLSID png{};
            if (pngEncoderClsid(png) != 0)
            {
                result = 5;
            }
            else
            {
                Bitmap image(bitmap, nullptr);
                if (image.Save(output.c_str(), &png, nullptr) != Ok)
                    result = 6;
            }
        }

        SelectObject(memoryDc, old);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        ReleaseDC(hwnd, windowDc);
        return result;
    }

    int runSession(HDESK desktop)
    {
        if (!ensureDesktopThread(desktop))
        {
            std::wcerr << L"Unable to attach controller thread to automation desktop.\n";
            return 1;
        }

        std::wcout << L"session-ready desktop=" << DesktopName << std::endl;
        std::wstring line;
        while (std::getline(std::wcin, line))
        {
            if (line.empty())
                continue;

            std::wistringstream input(line);
            std::wstring command;
            input >> command;
            if (command == L"quit" || command == L"exit")
            {
                std::wcout << L"session-closed" << std::endl;
                return 0;
            }

            auto window = findPaladin(desktop, 1000);
            if (!window.hwnd)
            {
                std::wcout << L"error=paladin-not-running" << std::endl;
                continue;
            }

            RECT rect{};
            GetClientRect(window.hwnd, &rect);
            const int width = int(rect.right - rect.left);
            const int height = int(rect.bottom - rect.top);

            auto normalize = [&](double& x, double& y)
            {
                x = std::clamp(x, 0.0, 1.0) * std::max(0, width - 1);
                y = std::clamp(y, 0.0, 1.0) * std::max(0, height - 1);
            };

            if (command == L"status")
            {
                std::wcout << L"ok status pid=" << window.pid << L" client="
                           << width << L"x" << height << std::endl;
                continue;
            }

            if (
                command == L"click" || command == L"clickn" ||
                command == L"rightclick" || command == L"rightclickn" ||
                command == L"move" || command == L"moven"
            )
            {
                double x = 0, y = 0;
                if (!(input >> x >> y))
                {
                    std::wcout << L"error=bad-point" << std::endl;
                    continue;
                }
                if (command.ends_with(L"n"))
                    normalize(x, y);
                const int px = int(x + 0.5), py = int(y + 0.5);
                SetForegroundWindow(window.hwnd);
                SetFocus(window.hwnd);
                postMouse(window.hwnd, WM_MOUSEMOVE, 0, px, py);
                if (command != L"move" && command != L"moven")
                {
                    const bool right = command.starts_with(L"rightclick");
                    postMouse(
                        window.hwnd,
                        right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                        right ? MK_RBUTTON : MK_LBUTTON,
                        px,
                        py
                    );
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    postMouse(
                        window.hwnd,
                        right ? WM_RBUTTONUP : WM_LBUTTONUP,
                        0,
                        px,
                        py
                    );
                }
                std::wcout << L"ok " << command << L" " << px << L" " << py
                           << std::endl;
                continue;
            }

            if (command == L"key" || command == L"keyhold")
            {
                std::wstring key;
                input >> key;
                const int vk = virtualKey(key);
                if (!vk)
                {
                    std::wcout << L"error=unsupported-key" << std::endl;
                    continue;
                }
                int holdMs = 25;
                if (command == L"keyhold")
                    input >> holdMs;
                holdMs = std::max(1, holdMs);
                SetForegroundWindow(window.hwnd);
                SetFocus(window.hwnd);
                postKey(window.hwnd, vk, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
                postKey(window.hwnd, vk, true);
                std::wcout << L"ok " << command << L" " << key << L" " << holdMs
                           << std::endl;
                continue;
            }

            if (command == L"wheel")
            {
                int x = 0, y = 0, delta = 0;
                if (!(input >> x >> y >> delta))
                {
                    std::wcout << L"error=bad-wheel" << std::endl;
                    continue;
                }
                POINT screen{x, y};
                ClientToScreen(window.hwnd, &screen);
                SetForegroundWindow(window.hwnd);
                SetFocus(window.hwnd);
                PostMessageW(
                    window.hwnd,
                    WM_MOUSEWHEEL,
                    MAKEWPARAM(0, static_cast<short>(delta)),
                    MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y))
                );
                std::wcout << L"ok wheel " << delta << std::endl;
                continue;
            }

            if (command == L"drag" || command == L"dragn")
            {
                double x1=0,y1=0,x2=0,y2=0;
                if (!(input >> x1 >> y1 >> x2 >> y2))
                {
                    std::wcout << L"error=bad-drag" << std::endl;
                    continue;
                }
                if (command == L"dragn")
                {
                    normalize(x1, y1);
                    normalize(x2, y2);
                }
                const int ax=int(x1+0.5), ay=int(y1+0.5);
                const int bx=int(x2+0.5), by=int(y2+0.5);
                SetForegroundWindow(window.hwnd);
                SetFocus(window.hwnd);
                postMouse(window.hwnd, WM_MOUSEMOVE, 0, ax, ay);
                postMouse(window.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, ax, ay);
                for (int i=1;i<=16;++i)
                {
                    const int x = ax + (bx-ax)*i/16;
                    const int y = ay + (by-ay)*i/16;
                    postMouse(window.hwnd, WM_MOUSEMOVE, MK_LBUTTON, x, y);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                postMouse(window.hwnd, WM_LBUTTONUP, 0, bx, by);
                std::wcout << L"ok " << command << std::endl;
                continue;
            }

            if (command == L"text")
            {
                std::wstring text;
                std::getline(input >> std::ws, text);
                SetForegroundWindow(window.hwnd);
                SetActiveWindow(window.hwnd);
                SetFocus(window.hwnd);
                for (wchar_t c : text)
                {
                    SendMessageW(window.hwnd, WM_CHAR, static_cast<WPARAM>(c), 1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                std::wcout << L"ok text " << text.size() << std::endl;
                continue;
            }

            if (command == L"screenshot")
            {
                std::wstring path;
                std::getline(input >> std::ws, path);
                std::filesystem::path output = path.empty()
                    ? rootPath() / L"debug" / L"runtime" / L"eye.png"
                    : std::filesystem::absolute(path);
                const int result = captureWindow(window.hwnd, output);
                if (result)
                    std::wcout << L"error=capture-" << result << std::endl;
                else
                    std::wcout << L"ok screenshot " << output.wstring() << std::endl;
                continue;
            }

            if (command == L"wait")
            {
                int ms = 0;
                input >> ms;
                std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, ms)));
                std::wcout << L"ok wait " << ms << std::endl;
                continue;
            }

            std::wcout << L"error=unknown-command" << std::endl;
        }
        return 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        std::wcerr << L"usage: PaladinDesktopControl <command> [...]\n";
        return 1;
    }

    HDESK desktop = openOrCreateDesktop();
    if (!desktop)
    {
        std::wcerr << L"Unable to open/create automation desktop: "
                   << GetLastError() << L"\n";
        return 2;
    }

    const std::wstring command = argv[1];

    if (command == L"session")
    {
        const int result = runSession(desktop);
        CloseDesktop(desktop);
        return result;
    }

    if (command == L"launch")
    {
        const bool restart = argc >= 3 && std::wstring(argv[2]) == L"restart";
        const int result = launch(desktop, restart);
        CloseDesktop(desktop);
        return result;
    }

    if (command == L"stop")
    {
        auto window = findPaladin(desktop, 200);
        if (!window.hwnd)
        {
            std::wcout << L"running=false desktop=" << DesktopName << L"\n";
            CloseDesktop(desktop);
            return 0;
        }

        PostMessageW(window.hwnd, WM_CLOSE, 0, 0);
        for (int i = 0; i < 30; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!IsWindow(window.hwnd))
                break;
        }
        if (IsWindow(window.hwnd))
        {
            HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, window.pid);
            if (process)
            {
                TerminateProcess(process, 0);
                CloseHandle(process);
            }
        }
        std::wcout << L"stopped pid=" << window.pid << L" desktop="
                   << DesktopName << L"\n";
        CloseDesktop(desktop);
        return 0;
    }

    auto window = findPaladin(desktop, 10000);
    if (!window.hwnd)
    {
        std::wcerr << L"Paladin is not running on the automation desktop.\n";
        CloseDesktop(desktop);
        return 3;
    }

    if (command == L"status")
    {
        RECT rect{};
        GetClientRect(window.hwnd, &rect);
        std::wcout << L"running=true pid=" << window.pid
                   << L" client=" << rect.right - rect.left
                   << L"x" << rect.bottom - rect.top
                   << L" desktop=" << DesktopName << L"\n";
    }
    else if (
        command == L"click" || command == L"clickn" ||
        command == L"rightclick" || command == L"rightclickn" ||
        command == L"move" || command == L"moven"
    )
    {
        if (argc < 4)
        {
            CloseDesktop(desktop);
            return 4;
        }
        double x = 0, y = 0;
        if (!parseDouble(argv[2], x) || !parseDouble(argv[3], y))
        {
            CloseDesktop(desktop);
            return 5;
        }
        RECT rect{};
        GetClientRect(window.hwnd, &rect);
        const bool normalized =
            command == L"clickn" || command == L"rightclickn" ||
            command == L"moven";
        if (normalized)
        {
            x = std::clamp(x, 0.0, 1.0) * std::max(0, int(rect.right) - 1);
            y = std::clamp(y, 0.0, 1.0) * std::max(0, int(rect.bottom) - 1);
        }
        const int px = static_cast<int>(x + 0.5);
        const int py = static_cast<int>(y + 0.5);

        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }
        postMouse(window.hwnd, WM_MOUSEMOVE, 0, px, py);
        if (command != L"move" && command != L"moven")
        {
            const bool right =
                command == L"rightclick" || command == L"rightclickn";
            postMouse(
                window.hwnd,
                right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                right ? MK_RBUTTON : MK_LBUTTON,
                px,
                py
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            postMouse(
                window.hwnd,
                right ? WM_RBUTTONUP : WM_LBUTTONUP,
                0,
                px,
                py
            );
        }
        std::wcout << (command == L"move" || command == L"moven" ? L"moved=" : L"clicked=")
                   << px << L"," << py << L" desktop=" << DesktopName << L"\n";
    }
    else if (command == L"key" || command == L"keyhold")
    {
        if (argc < 3)
        {
            CloseDesktop(desktop);
            return 6;
        }
        const int vk = virtualKey(argv[2]);
        if (!vk)
        {
            std::wcerr << L"Unsupported key.\n";
            CloseDesktop(desktop);
            return 7;
        }
        const int holdMs =
            command == L"keyhold" && argc >= 4 ? std::max(1, _wtoi(argv[3])) : 25;

        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }
        postKey(window.hwnd, vk, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
        postKey(window.hwnd, vk, true);
        std::wcout << L"key=" << argv[2] << L" hold_ms=" << holdMs
                   << L" desktop=" << DesktopName << L"\n";
    }
    else if (command == L"text")
    {
        if (argc < 3)
        {
            CloseDesktop(desktop);
            return 8;
        }
        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }
        const std::wstring text = argv[2];
        for (wchar_t c : text)
        {
            PostMessageW(window.hwnd, WM_CHAR, static_cast<WPARAM>(c), 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::wcout << L"text_chars=" << text.size() << L" desktop="
                   << DesktopName << L"\n";
    }
    else if (command == L"wheel")
    {
        if (argc < 5)
        {
            CloseDesktop(desktop);
            return 8;
        }
        double x = 0, y = 0;
        int delta = 0;
        if (!parseDouble(argv[2], x) || !parseDouble(argv[3], y) ||
            !parseInt(argv[4], delta))
        {
            CloseDesktop(desktop);
            return 9;
        }
        RECT rect{};
        GetClientRect(window.hwnd, &rect);
        const int px = static_cast<int>(x + 0.5);
        const int py = static_cast<int>(y + 0.5);
        POINT screen{px, py};
        ClientToScreen(window.hwnd, &screen);
        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }
        const WPARAM wheelParam = MAKEWPARAM(0, static_cast<short>(delta));
        PostMessageW(
            window.hwnd,
            WM_MOUSEWHEEL,
            wheelParam,
            MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y))
        );
        std::wcout << L"wheel=" << delta << L" at=" << px << L"," << py
                   << L" desktop=" << DesktopName << L"\n";
    }
    else if (command == L"drag" || command == L"dragn")
    {
        if (argc < 6)
        {
            CloseDesktop(desktop);
            return 8;
        }
        double x1=0,y1=0,x2=0,y2=0;
        if (!parseDouble(argv[2],x1) || !parseDouble(argv[3],y1) ||
            !parseDouble(argv[4],x2) || !parseDouble(argv[5],y2))
        {
            CloseDesktop(desktop);
            return 9;
        }
        RECT rect{};
        GetClientRect(window.hwnd, &rect);
        if (command == L"dragn")
        {
            x1 = std::clamp(x1,0.0,1.0) * std::max(0, int(rect.right)-1);
            y1 = std::clamp(y1,0.0,1.0) * std::max(0, int(rect.bottom)-1);
            x2 = std::clamp(x2,0.0,1.0) * std::max(0, int(rect.right)-1);
            y2 = std::clamp(y2,0.0,1.0) * std::max(0, int(rect.bottom)-1);
        }
        const int ax = static_cast<int>(x1 + 0.5);
        const int ay = static_cast<int>(y1 + 0.5);
        const int bx = static_cast<int>(x2 + 0.5);
        const int by = static_cast<int>(y2 + 0.5);

        if (ensureDesktopThread(desktop))
        {
            SetForegroundWindow(window.hwnd);
            SetFocus(window.hwnd);
        }

        postMouse(window.hwnd, WM_MOUSEMOVE, 0, ax, ay);
        postMouse(window.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, ax, ay);
        for (int i=1;i<=16;++i)
        {
            const int x = ax + (bx-ax)*i/16;
            const int y = ay + (by-ay)*i/16;
            postMouse(window.hwnd, WM_MOUSEMOVE, MK_LBUTTON, x, y);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        postMouse(window.hwnd, WM_LBUTTONUP, 0, bx, by);
        std::wcout << L"dragged=" << ax << L"," << ay << L"->"
                   << bx << L"," << by << L" desktop=" << DesktopName << L"\n";
    }
    else if (command == L"screenshot")
    {
        std::filesystem::path output;
        if (argc >= 3)
            output = std::filesystem::absolute(argv[2]);
        else
            output = rootPath() / L"debug" / L"runtime" / L"desktop-frame.png";

        const int result = captureWindow(window.hwnd, output);
        if (result)
        {
            std::wcerr << L"capture failed code=" << result << L"\n";
            CloseDesktop(desktop);
            return 10 + result;
        }
        std::wcout << output.wstring() << L"\n";
    }
    else
    {
        std::wcerr << L"Unknown command.\n";
        CloseDesktop(desktop);
        return 20;
    }

    CloseDesktop(desktop);
    return 0;
}
