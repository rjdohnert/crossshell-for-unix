#include "argument_formatter.hpp"
#include "watch_options.hpp"

int WatchOptions::Parse(int argc, wchar_t* argv[]) {
        bool afterDashDash = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (!afterDashDash && arg == L"--") {
                afterDashDash = true;
                continue;
            }

            if (!afterDashDash && (arg == L"-h" || arg == L"--help")) {
                showHelp = true;
                return 0;
            }

            if (!afterDashDash && arg == L"--version") {
                showVersion = true;
                return 0;
            }

            if (!afterDashDash && (arg == L"-t" || arg == L"--no-title")) {
                suppressHeader = true;
                continue;
            }

            if (!afterDashDash && (arg == L"-n" || arg == L"--interval")) {
                if (i + 1 >= argc) {
                    std::wcerr << L"watch: missing value for " << arg << L"\n";
                    return 2;
                }
                if (!ArgumentFormatter::ParseNumber(argv[++i], intervalSeconds)) {
                    std::wcerr << L"watch: invalid interval: " << argv[i] << L"\n";
                    return 2;
                }
                continue;
            }

            if (!afterDashDash && !arg.empty() && arg[0] == L'-') {
                std::wcerr << L"watch: unknown option -- " << arg << L"\n";
                return 2;
            }

            commandArgs.push_back(arg);
            for (++i; i < argc; ++i) {
                commandArgs.push_back(argv[i]);
            }
            break;
        }

        if (commandArgs.empty() && !showHelp && !showVersion) {
            std::wcerr << L"watch: missing command operand\n";
            return 2;
        }

        return 0;
    }
