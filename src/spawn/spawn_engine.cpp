#include "spawn_engine.hpp"
#include "process_controller.hpp"

SpawnEngine::SpawnEngine(SpawnOptions opts)
    : options(opts), reporter(opts.outputFormat, opts.pipeCommand) {}

int SpawnEngine::execute() {
    if (options.showHelp) {
        SpawnOptions::printHelp();
        return 0;
    }
    if (options.showVersion) {
        SpawnOptions::printVersion();
        return 0;
    }

    std::wstring fullCommand;
    wchar_t comspec[MAX_PATH];
    DWORD len = ::GetEnvironmentVariableW(L"COMSPEC", comspec, MAX_PATH);
    std::wstring shell = (len > 0 && len < MAX_PATH) ? comspec : L"cmd.exe";

    if (options.commandString.empty()) {
        fullCommand = shell;
    } else {
        fullCommand = shell + L" /c " + options.commandString;
    }

    DWORD exitCode = 0;
    bool ok = ProcessController::launch(fullCommand, options, reporter, exitCode);
    return ok ? static_cast<int>(exitCode) : 1;
}
