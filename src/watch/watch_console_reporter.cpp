#include "watch_console_reporter.hpp"
#include "watch_options.hpp"

void WatchConsoleReporter::ClearScreen() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

        COORD home = { 0, 0 };
        DWORD cellCount = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
        DWORD written = 0;

        FillConsoleOutputCharacterW(hOut, L' ', cellCount, home, &written);
        FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &written);
        SetConsoleCursorPosition(hOut, home);
    }

void WatchConsoleReporter::PrintHeader(const WatchOptions& options, const std::wstring& displayCommand) {
        if (options.suppressHeader) return;

        std::time_t now = std::time(nullptr);
        wchar_t timeBuf[64] = {};
        std::tm localTime = {};
        localtime_s(&localTime, &now);
        wcsftime(timeBuf, sizeof(timeBuf) / sizeof(timeBuf[0]), L"%Y-%m-%d %H:%M:%S", &localTime);

        std::wcout << L"Every " << options.intervalSeconds << L"s: " << displayCommand
                   << L"    " << timeBuf << L"\n\n";
    }

void WatchConsoleReporter::PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [options] command [args...]\n"
            << L"Run command repeatedly and display output fullscreen.\n\n"
            << L"Options:\n"
            << L"  -n, --interval SEC  Refresh interval in seconds (default: 2.0)\n"
            << L"  -t, --no-title      Do not show the header line\n"
            << L"  -h, --help          Display this help and exit\n"
            << L"      --version       Output version information and exit\n";
    }

void WatchConsoleReporter::PrintVersion() {
        std::wcout << L"watch v1.0.0\n";
    }
