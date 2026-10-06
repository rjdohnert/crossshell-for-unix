#include "admin_check.hpp"
#include "elevated_process.hpp"
#include "sudo_app.hpp"
#include "sudo_client.hpp"
#include "sudo_help.hpp"
#include "sudo_worker.hpp"
#include "target_command_line.hpp"
#include "terminal_mode.hpp"

int runSudo(int argc, wchar_t* argv[]) {
    EnableVTMode();

    if (argc < 2) {
        PrintUsage(L"sudo");
        return 0;
    }

    std::wstring firstArg = argv[1];

    // Handle Help flags
    if (firstArg == L"-h" || firstArg == L"--help") {
        PrintUsage(L"sudo");
        return 0;
    }

    if (firstArg == L"-v" || firstArg == L"--version") {
        PrintVersion();
        return 0;
    }

    if (firstArg == L"--validate") {
        std::wcout << L"sudo: environment validation passed\n";
        return 0;
    }

    // Check if worker mode triggered by internal runner
    if (firstArg == L"--sudo-worker") {
        if (argc < 4) return 1;
        std::wstring guid = argv[2];
        
        // Reconstruct target command line string
        std::wstring cmd = GetCommandLineW();
        size_t pos = cmd.find(guid);
        if (pos == std::wstring::npos) return 1;
        std::wstring targetCmd = cmd.substr(pos + guid.length() + 1);

        return RunWorker(guid, targetCmd);
    }

    std::wstring targetCmd = GetTargetCommandLine();

    // Direct launch if already running elevated
    if (IsAdmin()) {
        return RunElevatedDirect(targetCmd);
    }

    // Otherwise, trigger Client Proxy logic
    return RunClient(targetCmd);
}
