#include "script_options.hpp"

void ScriptOptions::printUsage(const wchar_t* progName) {
    std::wcout
        << L"Usage: " << progName << L" [OPTIONS] [OUTPUT] [COMMAND [ARGS...]]\n"
        << L"Record a command session transcript to a file.\n\n"
        << L"Options:\n"
        << L"  -a, --append      append to the output file instead of truncating it\n"
        << L"  -h, --help        display this help and exit\n"
        << L"  -V, --version     output version information and exit\n"
        << L"  --                end of options\n\n"
        << L"Notes:\n"
        << L"  If COMMAND is omitted, script launches cmd.exe.\n";
}

void ScriptOptions::printVersion() {
    std::wcout << L"script 1.0.0\n";
}

bool ScriptOptions::parse(int argc, wchar_t* argv[], ScriptOptions& opts) {
    bool passthrough = false;
    bool outputAssigned = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (passthrough) {
            opts.commandArgs.push_back(arg);
            continue;
        }

        if (arg == L"--") {
            passthrough = true;
            continue;
        }

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == L"-V" || arg == L"--version") {
            printVersion();
            std::exit(0);
        }
        if (arg == L"-a" || arg == L"--append") {
            opts.append = true;
            continue;
        }
        if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"script: unrecognized option: " << arg << L"\n";
            printUsage(argv[0]);
            return false;
        }

        if (!outputAssigned) {
            opts.outputFile = arg;
            outputAssigned = true;
            continue;
        }

        opts.commandArgs.push_back(arg);
        passthrough = true;
    }

    return true;
}
