#include "conexec.h"
#include "runcon_config.h"
#include "isolation_level.h"
#include "handle_closer.h"
#include "execution_dispatcher.h"

// --- CLI Parsing & Main Entry ---
int wmain(int argc, wchar_t* argv[]) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    RunconConfig cfg;
    bool parsingCommand = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (parsingCommand) {
            if (!cfg.command.empty()) cfg.command += L" ";
            cfg.command += EscapeArgument(arg);
            continue;
        }

        if (arg == L"-h" || arg == L"--help") {
            PrintHelp();
            return 0;
        } else if (arg == L"-V" || arg == L"--version") {
            PrintVersion();
            return 0;
        } else if (arg == L"-5") {
            cfg.level = IsolationLevel::Normal;
        } else if (arg == L"-4") {
            cfg.level = IsolationLevel::TempWorkspace;
        } else if (arg == L"-3") {
            cfg.level = IsolationLevel::PromptCommit;
        } else if (arg == L"-2") {
            cfg.level = IsolationLevel::EphemeralNet;
        } else if (arg == L"-1") {
            cfg.level = IsolationLevel::TotalLockdown;
        } else if (arg == L"-s" || arg == L"--stage") {
            cfg.stageCurrentDir = true;
        } else if (arg == L"-v" || arg == L"--verbose") {
            cfg.verbose = true;
        } else if ((arg == L"-d" || arg == L"--dir") && (i + 1 < argc)) {
            cfg.workingDirectory = argv[++i];
        } else if (arg == L"--") {
            parsingCommand = true;
        } else {
            parsingCommand = true;
            cfg.command += EscapeArgument(arg);
        }
    }

    if (cfg.command.empty()) {
        std::wcerr << L"\033[31m[-] Error: No execution target specified.\033[0m\n";
        std::wcerr << L"    Run 'conexec --help' for syntax.\n";
        return 1;
    }

    return ExecutionDispatcher::Execute(cfg);
}
