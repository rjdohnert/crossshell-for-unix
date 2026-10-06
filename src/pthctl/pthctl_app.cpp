#include "pthctl_app.hpp"

int PthctlApp::run(int argc, wchar_t* argv[]) {
    PthctlParsedArgs parsed;
    if (!PthctlOptions::Parse(argc, argv, parsed)) {
        return 1;
    }

    if (parsed.shouldExit) {
        return parsed.exitCode;
    }

    PthOutputSession outputSession(parsed.outputFormat, parsed.pipeCommand);

    // Command Router
    if (parsed.command == L"list" || parsed.command == L"ls") {
        if (!parsed.pathArg.empty()) {
            std::wcerr << L"pthctl: error: 'list' does not accept a path argument.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return PthctlEngine::CommandList(parsed.scope);
    } else if (parsed.command == L"add") {
        if (parsed.pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'add' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return PthctlEngine::CommandAdd(parsed.scope, parsed.pathArg, parsed.prepend, parsed.dryRun);
    } else if (parsed.command == L"remove" || parsed.command == L"rm") {
        if (parsed.pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'remove' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return PthctlEngine::CommandRemove(parsed.scope, parsed.pathArg, parsed.dryRun);
    } else if (parsed.command == L"check") {
        if (parsed.pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'check' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return PthctlEngine::CommandCheck(parsed.scope, parsed.pathArg);
    } else if (parsed.command == L"clean") {
        if (!parsed.pathArg.empty()) {
            std::wcerr << L"pthctl: error: 'clean' does not accept a path argument.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return PthctlEngine::CommandClean(parsed.scope, parsed.dryRun);
    } else {
        std::wcerr << L"pthctl: unknown command '" << parsed.command << L"'\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return 1;
    }
}
