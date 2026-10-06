#include "runcon_options.hpp"

void RunconOptions::printUsage(const wchar_t* progName) {
    (void)progName;
    std::wcout << LR"(runcon(1)                CrossShell for UNIX Reference Manual                   runcon(1)

NAME
    runcon - run a command at a Windows integrity level

SYNOPSIS
    runcon [CONTEXT] COMMAND [ARGS...]
    runcon -l LEVEL COMMAND [ARGS...]

DESCRIPTION
    Launches COMMAND with a duplicated Windows token configured with the
    requested mandatory integrity level. Contexts may use
    [USER:][ROLE:][TYPE:][LEVEL]; only LEVEL is enforced.

INTEGRITY LEVELS
    untrusted, 0       S-1-16-0
    low, l, 4096       S-1-16-4096
    medium, m, 8192    S-1-16-8192
    high, h, 12288     S-1-16-12288
    system, s, 16384   S-1-16-16384

OPTIONS
    -l, --range LEVEL  Set the integrity level explicitly.
    -h, --help, /?     Display this comprehensive reference manual and exit.
    --version          Display version information and exit.
    --                 End options and introduce the command.

EXAMPLES
    runcon low notepad.exe
    runcon -l high cmd.exe /c whoami
    runcon system service.exe -- --safe

EXIT STATUS
    0          Help, version, or the child process's exit code.
    1          Invalid level, option, token, SID, or launch failure.

CrossShell for UNIX                                                       runcon(1)
)";
}

void RunconOptions::printVersion() {
    std::wcout << L"runcon 1.0.0\n";
}

bool RunconOptions::parse(int argc, wchar_t* argv[], RunconOptions& opts) {
    int i = 1;
    for (; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == L"--version") {
            printVersion();
            std::exit(0);
        } else if ((arg == L"-l" || arg == L"--range") && i + 1 < argc) {
            opts.level = argv[++i];
        } else if (arg == L"--") {
            ++i;
            break;
        } else if (arg[0] == L'-') {
            std::wcerr << L"runcon: invalid option -- '" << arg << L"'\n";
            return false;
        } else {
            break;
        }
    }

    if (opts.level.empty() && i < argc) {
        std::wstring context = argv[i++];
        size_t lastColon = context.rfind(L':');
        if (lastColon != std::wstring::npos) {
            opts.level = context.substr(lastColon + 1);
        } else {
            opts.level = context;
        }
    }

    for (; i < argc; ++i) {
        opts.commandArgs.push_back(argv[i]);
    }

    if (opts.commandArgs.empty()) {
        std::wcerr << L"runcon: you must specify a command to run\n";
        return false;
    }

    return true;
}
