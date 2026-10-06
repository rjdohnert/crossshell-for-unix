#include "options.hpp"

void NewshellOptionsParser::PrintVersion() {
    std::wcout << L"newshell 3.0.0\n"
               << L"Copyright (C) 2026, Roberto J Dohnert\n";
}

void NewshellOptionsParser::PrintHelp() {
    std::wcout << L"newshell - Launch modern or legacy Windows terminals on demand.\n\n"
               << L"USAGE:\n"
               << L"    newshell.exe [OPTIONS] [-- <command> [args...]]\n\n"
               << L"DESCRIPTION:\n"
               << L"    Launches a new instance of Windows Terminal (wt.exe). If Windows\n"
               << L"    Terminal is not installed or detected in PATH, it seamlessly falls\n"
               << L"    back to the classic Windows Console Host (conhost.exe).\n\n"
               << L"OPTIONS:\n"
               << L"    -a, --admin          Prompt for UAC elevation to launch an Administrator terminal.\n"
               << L"    -d, --dir <PATH>     Set the initial working directory for the terminal session.\n"
               << L"    -p, --profile <NAME> Specify a Windows Terminal profile (ignored in conhost fallback).\n"
               << L"    -f, --force-conhost  Force classic Console Host (conhost.exe) instead of Windows Terminal.\n"
               << L"    -h, --help           Display this comprehensive help menu and exit.\n"
               << L"    -v, --version        Display version and copyright information and exit.\n\n"
               << L"EXAMPLES:\n"
               << L"    newshell\n"
               << L"        Opens a new Windows Terminal (or conhost fallback) in the current directory.\n\n"
               << L"    newshell --admin\n"
               << L"        Triggers a UAC prompt and opens an elevated Administrator terminal.\n\n"
               << L"    newshell --dir \"C:\\Projects\" --admin\n"
               << L"        Opens an elevated terminal rooted at C:\\Projects.\n\n"
               << L"    newshell --force-conhost --admin\n"
               << L"        Forces classic elevated conhost.exe session.\n\n"
               << L"    newshell -- ping 1.1.1.1 -t\n"
               << L"        Launches a new terminal executing the specified command.\n";
}

bool NewshellOptionsParser::Parse(int argc, wchar_t* argv[], NewshellOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            opts.showHelp = true;
            return true;
        } else if (arg == L"-v" || arg == L"--version") {
            opts.showVersion = true;
            return true;
        } else if (arg == L"-a" || arg == L"--admin") {
            opts.runAsAdmin = true;
        } else if (arg == L"-f" || arg == L"--force-conhost") {
            opts.forceConhost = true;
        } else if ((arg == L"-d" || arg == L"--dir") && i + 1 < argc) {
            opts.workingDir = argv[++i];
        } else if ((arg == L"-p" || arg == L"--profile") && i + 1 < argc) {
            opts.profileName = argv[++i];
        } else if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                if (!opts.customCommand.empty()) opts.customCommand += L" ";
                std::wstring cmdPart = argv[j];
                if (cmdPart.find(L' ') != std::wstring::npos) {
                    opts.customCommand += L"\"" + cmdPart + L"\"";
                } else {
                    opts.customCommand += cmdPart;
                }
            }
            break;
        }
    }
    return true;
}
