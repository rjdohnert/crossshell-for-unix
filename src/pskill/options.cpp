#include "options.hpp"

std::wstring PskillOptions::ToLower(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::towlower);
    return str;
}

bool PskillOptions::TryParsePID(const std::wstring& str, DWORD& pid) {
    if (str.empty()) return false;
    for (wchar_t c : str) {
        if (!std::iswdigit(c)) return false;
    }
    try {
        pid = std::stoul(str);
        return true;
    } catch (...) {
        return false;
    }
}

int PskillOptions::Parse(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        showHelp = true;
        return EXIT_INVALID_ARGS;
    }

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        std::wstring key = arg;
        std::wstring val = L"";

        size_t eqPos = arg.find(L'=');
        if (eqPos != std::wstring::npos) {
            key = arg.substr(0, eqPos);
            val = arg.substr(eqPos + 1);
        }

        if (key == L"-h" || key == L"--help" || key == L"/?") {
            showHelp = true;
            return EXIT_SUCCESS_OK;
        } else if (key == L"-f" || key == L"--force") {
            force = true;
            signal = SIG_KILL;
        } else if (key == L"-t" || key == L"--tree") {
            tree = true;
        } else if (key == L"-e" || key == L"--exact") {
            exact = true;
        } else if (key == L"-c" || key == L"--case-sensitive") {
            caseSensitive = true;
        } else if (key == L"-n" || key == L"--dry-run") {
            dryRun = true;
        } else if (key == L"-v" || key == L"--verbose") {
            verbose = true;
        } else if (key == L"-q" || key == L"--quiet") {
            quiet = true;
        } else if (key == L"-u" || key == L"--user") {
            if (!val.empty()) {
                userFilter = val;
            } else if (i + 1 < argc) {
                userFilter = argv[++i];
            } else {
                std::wcerr << L"Error: Option " << key << L" requires a user argument.\n";
                return EXIT_INVALID_ARGS;
            }
        } else if (key == L"-s" || key == L"--signal") {
            std::wstring sigStr = val;
            if (sigStr.empty() && i + 1 < argc) {
                sigStr = argv[++i];
            }
            if (sigStr == L"9" || sigStr == L"KILL" || sigStr == L"SIGKILL") {
                signal = SIG_KILL;
            } else if (sigStr == L"15" || sigStr == L"TERM" || sigStr == L"SIGTERM") {
                signal = SIG_TERM;
            } else if (sigStr == L"2" || sigStr == L"INT" || sigStr == L"SIGINT") {
                signal = SIG_INT;
            } else {
                std::wcerr << L"Error: Unsupported signal identifier: " << sigStr << L"\n";
                return EXIT_INVALID_ARGS;
            }
        } else if (key.rfind(L"-", 0) == 0) {
            std::wcerr << L"Error: Unrecognized option '" << key << L"'. Run 'pskill --help' for usage.\n";
            return EXIT_INVALID_ARGS;
        } else {
            targets.push_back(arg);
        }
    }

    if (targets.empty()) {
        std::wcerr << L"Error: No target processes specified.\n";
        return EXIT_INVALID_ARGS;
    }

    return -1;
}

void PskillOptions::PrintHelp() const {
    std::wcout << LR"(pskill(1)               CrossShell for UNIX Reference Manual                pskill(1)

    NAME
        pskill - kill processes by name, PID, or remote computer

    SYNOPSIS
        pskill [OPTIONS] [\\COMPUTER] [PID|PROCESS_NAME]

    DESCRIPTION
        pskill terminates processes running on the local Windows system or on a
        remote Windows computer using standard Win32 / WMI management APIs.

    OPTIONS
        -t, --tree
            Kill the process and all of its descendant child processes.

        -u USER, --user USER
            Target only processes running under the specified username.

        --json, --csv, --table
            Format termination status as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        pskill -t 1234
            Kill process 1234 and its complete process sub-tree.

        pskill calc.exe
            Kill all running Calculator instances.

    CrossShell for UNIX                                                 pskill(1)
)";
}
