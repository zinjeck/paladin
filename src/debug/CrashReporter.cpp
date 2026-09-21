#include "debug/CrashReporter.h"
#include "debug/CrashContext.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// DbgHelp requires the Windows declarations above.
#include <DbgHelp.h>
#include <algorithm>
#include <csignal>
#include <cwchar>
#include <filesystem>
#include <vector>
#endif

namespace Paladin
{
    namespace
    {
        std::atomic_flag reporting = ATOMIC_FLAG_INIT;
        wchar_t reportDirectory[2048]{};
#if defined(_WIN32)
        wchar_t reportPath[2304]{}, dumpPath[2304]{};
        using DumpWriter = decltype(&MiniDumpWriteDump);
        DumpWriter writeDump = nullptr;
        std::atomic<bool> installed{false};

        // Retention runs only during healthy startup, never in the exception
        // handler. Delete only our exact report names in our dedicated folder.
        void pruneOldReports()
        {
            namespace fs = std::filesystem;
            std::vector<fs::path> reports;
            std::error_code error;
            for (const auto& entry :
                 fs::directory_iterator(reportDirectory, error))
            {
                const auto name = entry.path().filename().wstring();
                const bool ownName =
                    name.starts_with(L"Paladin-") && name.ends_with(L".txt") &&
                    name.size() >= 35 &&
                    std::all_of(
                        name.begin() + 8,
                        name.end() - 4,
                        [](wchar_t c)
                        { return (c >= L'0' && c <= L'9') || c == L'-'; }
                    );
                if (entry.is_regular_file(error) && ownName)
                {
                    reports.push_back(entry.path());
                }
            }
            std::sort(reports.begin(), reports.end());
            constexpr std::size_t maximumReports = 8;
            while (reports.size() >= maximumReports)
            {
                auto path = reports.front();
                fs::remove(path, error);
                path.replace_extension(L".dmp");
                fs::remove(path, error);
                reports.erase(reports.begin());
            }
        }

        void report(const char* message, EXCEPTION_POINTERS* exception) noexcept
        {
            if (reporting.test_and_set() || !reportPath[0])
            {
                return;
            }
            // Paths and DbgHelp were prepared while the process was healthy.
            // Open the text report first so a failed dump still leaves context.
            const auto file = CreateFileW(
                reportPath,
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
            if (file != INVALID_HANDLE_VALUE)
            {
                char text[8192]{};
                MEMORYSTATUSEX memory{};
                memory.dwLength = sizeof(memory);
                GlobalMemoryStatusEx(&memory);
                const DWORD code =
                    exception && exception->ExceptionRecord
                        ? exception->ExceptionRecord->ExceptionCode
                        : 0;
                const void* address =
                    exception && exception->ExceptionRecord
                        ? exception->ExceptionRecord->ExceptionAddress
                        : nullptr;
                const int length = std::snprintf(
                    text,
                    sizeof(text),
                    "Paladin local crash report\r\n"
                    "Build: %s %s (%s)\r\n"
                    "Reason: %.2048s\r\nException: 0x%08lX at %p\r\n"
                    "Process: %lu Thread: %lu Uptime ms: %llu\r\n"
                    "Screen: %s\r\nPhase: %s\r\n"
                    "World seed: %llu\r\nGame minute: %llu\r\nTick: %llu\r\n"
                    "Active settlement: %llu\r\nSettlements: %llu\r\n"
                    "Soldiers: %llu Armies: %llu Shipments: %llu\r\n"
                    "Tick game minutes: %.6f Last tick ms: %.3f\r\n"
                    "Available physical memory bytes: %llu\r\n"
                    "Memory load: %lu%%\r\n"
                    "A companion .dmp contains Windows thread stacks and "
                    "module details.\r\n"
                    "No report or game data has been uploaded.\r\n",
                    __DATE__,
                    __TIME__,
#ifdef _DEBUG
                    "Debug",
#else
                    "Release",
#endif
                    message ? message : "Unhandled native exception",
                    code,
                    address,
                    GetCurrentProcessId(),
                    GetCurrentThreadId(),
                    GetTickCount64(),
                    CrashContext::screen.load(),
                    CrashContext::phase.load(),
                    CrashContext::seed.load(),
                    CrashContext::minute.load(),
                    CrashContext::tick.load(),
                    CrashContext::activeSettlement.load(),
                    CrashContext::settlements.load(),
                    CrashContext::soldiers.load(),
                    CrashContext::armies.load(),
                    CrashContext::shipments.load(),
                    CrashContext::elapsedMinutes.load(),
                    CrashContext::tickMilliseconds.load(),
                    memory.ullAvailPhys,
                    memory.dwMemoryLoad
                );
                DWORD written = 0;
                if (length > 0)
                {
                    WriteFile(
                        file,
                        text,
                        DWORD(std::min(length, int(sizeof(text) - 1))),
                        &written,
                        nullptr
                    );
                }
                FlushFileBuffers(file);
                CloseHandle(file);
            }
            if (writeDump)
            {
                const auto dump = CreateFileW(
                    dumpPath,
                    GENERIC_WRITE,
                    FILE_SHARE_READ,
                    nullptr,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr
                );
                if (dump != INVALID_HANDLE_VALUE)
                {
                    MINIDUMP_EXCEPTION_INFORMATION info{
                        GetCurrentThreadId(),
                        exception,
                        FALSE
                    };
                    writeDump(
                        GetCurrentProcess(),
                        GetCurrentProcessId(),
                        dump,
                        MINIDUMP_TYPE(
                            MiniDumpNormal | MiniDumpWithThreadInfo |
                            MiniDumpWithUnloadedModules
                        ),
                        exception ? &info : nullptr,
                        nullptr,
                        nullptr
                    );
                    CloseHandle(dump);
                }
            }
            std::fwprintf(stderr, L"Paladin crash report: %ls\n", reportPath);
        }
        LONG WINAPI unhandled(EXCEPTION_POINTERS* exception)
        {
            report("Unhandled Windows exception", exception);
            return EXCEPTION_EXECUTE_HANDLER;
        }
        void invalidParameter(
            const wchar_t*,
            const wchar_t*,
            const wchar_t*,
            unsigned int,
            std::uintptr_t
        )
        {
            CrashReporter::reportFatal("Invalid C runtime parameter");
            TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
        }
        void aborted(int)
        {
            CrashReporter::reportFatal(
                "Process aborted (assertion or SIGABRT)"
            );
            TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
        }
#endif
        void terminated() noexcept
        {
            const char* message = "std::terminate without an active exception";
            try
            {
                if (const auto exception = std::current_exception())
                {
                    std::rethrow_exception(exception);
                }
            }
            catch (const std::exception& exception)
            {
                CrashReporter::reportFatal(exception.what());
                std::_Exit(EXIT_FAILURE);
            }
            catch (...)
            {
                message = "Unhandled non-standard C++ exception";
            }
            CrashReporter::reportFatal(message);
            std::_Exit(EXIT_FAILURE);
        }
    } // namespace

    void CrashReporter::install() noexcept
    {
#if defined(_WIN32)
        if (installed.exchange(true))
        {
            return;
        }
        try
        {
            wchar_t base[1800]{};
            const DWORD custom =
                GetEnvironmentVariableW(L"PALADIN_CRASH_DIR", base, 1800);
            if (custom > 0 && custom < 1800)
            {
                std::swprintf(reportDirectory, 2048, L"%ls", base);
            }
            else
            {
                const DWORD size =
                    GetEnvironmentVariableW(L"LOCALAPPDATA", base, 1800);
                if (!size || size >= 1800)
                {
                    GetTempPathW(1800, base);
                }
                std::swprintf(
                    reportDirectory,
                    2048,
                    L"%ls\\Paladin\\CrashReports",
                    base
                );
            }
            std::filesystem::create_directories(reportDirectory);
            pruneOldReports();
            SYSTEMTIME now{};
            GetSystemTime(&now);
            wchar_t stem[2240]{};
            std::swprintf(
                stem,
                2240,
                L"%ls\\Paladin-%04u%02u%02u-%02u%02u%02u-%03u-%lu",
                reportDirectory,
                now.wYear,
                now.wMonth,
                now.wDay,
                now.wHour,
                now.wMinute,
                now.wSecond,
                now.wMilliseconds,
                GetCurrentProcessId()
            );
            std::swprintf(reportPath, 2304, L"%ls.txt", stem);
            std::swprintf(dumpPath, 2304, L"%ls.dmp", stem);
            if (const auto library = LoadLibraryExW(
                    L"dbghelp.dll",
                    nullptr,
                    LOAD_LIBRARY_SEARCH_SYSTEM32
                ))
            {
                writeDump = reinterpret_cast<DumpWriter>(
                    GetProcAddress(library, "MiniDumpWriteDump")
                );
            }
        }
        catch (...)
        {
            // Reporting must never prevent the game from opening.
            reportPath[0] = dumpPath[0] = 0;
        }
        ULONG reserve = 64 * 1024;
        SetThreadStackGuarantee(&reserve);
        SetUnhandledExceptionFilter(unhandled);
        _set_invalid_parameter_handler(invalidParameter);
        std::signal(SIGABRT, aborted);
#endif
        std::set_terminate(terminated);
    }

    void CrashReporter::reportFatal(const char* message) noexcept
    {
#if defined(_WIN32)
        CONTEXT context{};
        RtlCaptureContext(&context);
        EXCEPTION_RECORD record{};
        record.ExceptionCode = 0xE0000001;
        EXCEPTION_POINTERS exception{&record, &context};
        report(message, &exception);
#else
        std::fprintf(
            stderr,
            "Paladin fatal error: %s\n",
            message ? message : "Unknown"
        );
#endif
    }
    const wchar_t* CrashReporter::directory() noexcept
    {
        return reportDirectory;
    }
} // namespace Paladin
