#include "spawn_options.hpp"
#include "vms_status.hpp"

void SpawnOptions::printHelp() {
    std::wcout <<
LR"(SPAWN

    Creates a concurrent subprocess and optionally transfers control to it.

Format:

    SPAWN  [command-string]

Command Parameters:

    command-string
        Specifies a command string to be executed in the context of the
        created subprocess. If omitted, an interactive subshell is spawned.

Command Qualifiers:

    /INPUT=file-spec
        Directs standard input from the specified file.

    /OUTPUT=file-spec
        Directs standard output to the specified file.

    /PROCESS_NAME=process-name
        Specifies a descriptive name for the spawned process.

    /WAIT (default)
    /NOWAIT
        Controls whether SPAWN waits for the subprocess to complete before
        returning control to the parent command line.

    /NOTIFY
    /NONOTIFY (default)
        Broadcasts a completion message when a /NOWAIT subprocess finishes.

    /HELP
        Displays this help documentation.

Examples:

    SPAWN
    SPAWN/NOWAIT/NOTIFY/OUTPUT=build.log nmake
    SPAWN/PROCESS_NAME="BackupTask" robocopy C:\Data D:\Backup /MIR
)";
}

void SpawnOptions::printVersion() {
    std::wcout << L"SPAWN version 1.1.0\n";
}

bool SpawnOptions::parse(int argc, wchar_t* argv[], SpawnOptions& opts) {
    std::vector<std::wstring> rawArgs;
    for (int i = 1; i < argc; ++i) {
        rawArgs.emplace_back(argv[i]);
    }

    std::vector<std::wstring> cmdTokens;

    for (size_t i = 0; i < rawArgs.size(); ++i) {
        const auto& arg = rawArgs[i];

        if (arg == L"/?" || arg == L"-h" || arg == L"--help" || VmsStatusReporter::toUpper(arg) == L"/HELP") {
            opts.showHelp = true;
            return true;
        }
        if (arg == L"-V" || arg == L"--version" || VmsStatusReporter::toUpper(arg) == L"/VERSION") {
            opts.showVersion = true;
            return true;
        }

        if (!arg.empty() && (arg[0] == L'/' || arg[0] == L'-')) {
            std::wstring uArg = VmsStatusReporter::toUpper(arg);

            if (uArg == L"/WAIT" || uArg == L"--WAIT") {
                opts.wait = true;
            } else if (uArg == L"/NOWAIT" || uArg == L"--NOWAIT") {
                opts.wait = false;
            } else if (uArg == L"/NOTIFY" || uArg == L"--NOTIFY") {
                opts.notify = true;
            } else if (uArg == L"/NONOTIFY" || uArg == L"--NONOTIFY") {
                opts.notify = false;
            } else if (uArg.rfind(L"/PROCESS_NAME=", 0) == 0 || uArg.rfind(L"--PROCESS_NAME=", 0) == 0) {
                size_t eq = arg.find(L'=');
                opts.processName = arg.substr(eq + 1);
            } else if (uArg.rfind(L"/INPUT=", 0) == 0 || uArg.rfind(L"--INPUT=", 0) == 0) {
                size_t eq = arg.find(L'=');
                opts.inputFile = arg.substr(eq + 1);
            } else if (uArg.rfind(L"/OUTPUT=", 0) == 0 || uArg.rfind(L"--OUTPUT=", 0) == 0) {
                size_t eq = arg.find(L'=');
                opts.outputFile = arg.substr(eq + 1);
            } else {
                cmdTokens.push_back(arg);
            }
        } else {
            cmdTokens.push_back(arg);
        }
    }

    if (!cmdTokens.empty()) {
        std::wstring joined;
        for (size_t i = 0; i < cmdTokens.size(); ++i) {
            if (i > 0) joined += L" ";
            cmdTokens[i].erase(std::remove(cmdTokens[i].begin(), cmdTokens[i].end(), L'\r'), cmdTokens[i].end());
            joined += cmdTokens[i];
        }
        opts.commandString = joined;
    }

    return true;
}
