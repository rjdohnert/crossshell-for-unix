#include "refresh_options.hpp"

bool RefreshOptions::Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-v" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-s" && i + 1 < argc) {
                service = argv[++i];
                continue;
            }
            if (arg == L"-r" || arg == L"--restart") {
                restartFallback = true;
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"refresh: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"refresh: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }

void RefreshOptions::PrintHelp() const {
        std::cout << R"(refresh(1)                CrossShell for UNIX Reference Manual                refresh(1)

    NAME
        refresh - refresh a Windows service configuration

    SYNOPSIS
        refresh -s SERVICE [OPTIONS]

    DESCRIPTION
        refresh sends the SERVICE_CONTROL_PARAMCHANGE control signal to a
        Windows service to reload its configuration dynamically. With -r,
        it falls back to stopping and restarting the service if parameter
        change is not supported.

    OPTIONS
        -s <service>
            Specify the name of the service to refresh (required).

        -r, --restart
            Restart the service if parameter-change control is unsupported.

        -t <seconds>
            Set the operation timeout in seconds (default: 30).

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        refresh -s Spooler
            Refresh the Print Spooler service configuration.

        refresh -s W32Time -r
            Refresh time service or restart if param-change is unsupported.

        refresh -s MyService -t 60
            Refresh MyService with a 60-second wait timeout.

    CrossShell for UNIX                                                          refresh(1)
)";
    }

void RefreshOptions::PrintVersion() const {
        std::cout << "refresh v1.0.0\n";
    }
