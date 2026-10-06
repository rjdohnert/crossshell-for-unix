#include "newshell_app.hpp"

int NewshellApp::Run(int argc, wchar_t* argv[]) {
    NewshellOptions options;
    if (!NewshellOptionsParser::Parse(argc, argv, options)) {
        return 1;
    }

    if (options.showHelp) {
        NewshellOptionsParser::PrintHelp();
        return 0;
    }

    if (options.showVersion) {
        NewshellOptionsParser::PrintVersion();
        return 0;
    }

    if (!options.workingDir.empty()) {
        DWORD attributes = GetFileAttributesW(options.workingDir.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wcerr << L"[newshell] Warning: Working directory does not exist. Using current directory.\n";
            options.workingDir = L"";
        } else {
            DWORD required = GetFullPathNameW(options.workingDir.c_str(), 0, nullptr, nullptr);
            if (required > 0) {
                std::vector<wchar_t> fullPath(required);
                DWORD written = GetFullPathNameW(options.workingDir.c_str(), required, fullPath.data(), nullptr);
                if (written > 0 && written < required) {
                    options.workingDir.assign(fullPath.data(), written);
                }
            }
        }
    }

    std::wstring targetExe;
    std::wstring targetArgs;

    if (!options.forceConhost && TerminalLauncher::IsWindowsTerminalAvailable()) {
        targetExe = L"wt.exe";
        if (!options.profileName.empty()) {
            targetArgs += L"-p \"" + options.profileName + L"\" ";
        }
        if (!options.workingDir.empty()) {
            targetArgs += L"-d \"" + options.workingDir + L"\" ";
        }
        if (!options.customCommand.empty()) {
            targetArgs += options.customCommand;
        }
    } else {
        targetExe = L"conhost.exe";
        if (!options.customCommand.empty()) {
            targetArgs = L"cmd.exe /k " + options.customCommand;
        } else {
            targetArgs = L"cmd.exe";
        }
    }

    bool success = TerminalLauncher::Launch(targetExe, targetArgs, options.workingDir, options.runAsAdmin);
    return success ? 0 : 1;
}
