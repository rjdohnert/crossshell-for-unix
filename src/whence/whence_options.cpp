#include "whence_options.hpp"

void WhenceOptions::printHelp(const std::wstring& progName) {
        std::wcout << L"Usage: " << progName << L" [-v] [-a] [-p] name ...\n\n"
                   << L"Locate a command, resolve its path, and identify its type.\n\n"
                   << L"Options:\n"
                   << L"  -v, --verbose   produce verbose classification output\n"
                   << L"  -a, --all       display all occurrences in PATH\n"
                   << L"  -p, --path      force PATH lookup (ignore builtins)\n"
                   << L"      --json      output classification records as JSON\n"
                   << L"      --csv       output classification records as CSV\n"
                   << L"      --table     output classification records as a table\n"
                   << L"      --pipe CMD  send output through CMD\n"
                   << L"  -h, --help      display this help message and exit\n";
    }

bool WhenceOptions::parse(int argc, wchar_t* argv[], WhenceOptions& opts) {
        bool stopFlags = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (stopFlags) {
                opts.targets.push_back(arg);
                continue;
            }

            if (arg == L"--") {
                stopFlags = true;
                continue;
            }

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-a" || arg == L"--all") {
                opts.showAll = true;
            } else if (arg == L"-p" || arg == L"--path") {
                opts.pathSearchOnly = true;
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    if (arg[j] == L'v') opts.verbose = true;
                    else if (arg[j] == L'a') opts.showAll = true;
                    else if (arg[j] == L'p') opts.pathSearchOnly = true;
                    else {
                        std::wcerr << L"whence: unknown option -- " << arg[j] << L"\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            printHelp(argv[0]);
            return false;
        }

        return true;
    }
