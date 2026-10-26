#include "xmic_common.hpp"

int RunInteractiveShell(const std::wstring& executablePath) {
    std::wcout << L"\nXMIC interactive shell" << std::endl;
    std::wcout << L"Enter a normal xmic command, one per line." << std::endl;
    std::wcout << L"Examples: os, process, path Win32_Process get Name,ProcessId" << std::endl;
    std::wcout << L"Type 'help' for shell commands, 'set <name> <value>' for defaults, or 'exit' / 'quit' to leave." << std::endl;

    std::wstring shellNode;
    std::wstring shellUser;
    std::wstring shellPassword;
    std::wstring shellDomain;
    std::wstring shellNamespace;
    std::wstring shellTransport;
    std::wstring shellWinrmBackend;
    std::wstring shellFormat;
    std::wstring shellConsoleEncoding;
    bool shellNoHeader = false;

    while (true) {
        std::wcout << L"xmic> ";
        std::wstring inputLine;
        if (!std::getline(std::wcin, inputLine)) {
            break;
        }

        if (inputLine.empty()) {
            continue;
        }

        std::vector<std::wstring> tokens = TokenizeShellInput(inputLine);
        if (tokens.empty()) {
            continue;
        }

        std::wstring command = tokens[0];
        std::transform(command.begin(), command.end(), command.begin(), towlower);

        if (command == L"exit" || command == L"quit") {
            break;
        }

        if (command == L"help") {
            std::wcout << L"Shell commands:" << std::endl;
            std::wcout << L"  help                      Show this help" << std::endl;
            std::wcout << L"  exit / quit               Leave the shell" << std::endl;
            std::wcout << L"  set <name> <value>        Set persistent shell options" << std::endl;
            std::wcout << L"  show                      Show current shell options" << std::endl;
            std::wcout << L"Command examples:" << std::endl;
            std::wcout << L"  os" << std::endl;
            std::wcout << L"  process" << std::endl;
            std::wcout << L"  path Win32_Process get Name,ProcessId" << std::endl;
            std::wcout << L"Common defaults:" << std::endl;
            std::wcout << L"  set node server01" << std::endl;
            std::wcout << L"  set transport ssh" << std::endl;
            std::wcout << L"  set format json" << std::endl;
            continue;
        }

        if (command == L"show") {
            std::wcout << L"Current shell options:" << std::endl;
            if (!shellNode.empty()) std::wcout << L"  node=" << shellNode << std::endl;
            if (!shellUser.empty()) std::wcout << L"  user=" << shellUser << std::endl;
            if (!shellPassword.empty()) std::wcout << L"  password=<set>" << std::endl;
            if (!shellDomain.empty()) std::wcout << L"  domain=" << shellDomain << std::endl;
            if (!shellNamespace.empty()) std::wcout << L"  namespace=" << shellNamespace << std::endl;
            if (!shellTransport.empty()) std::wcout << L"  transport=" << shellTransport << std::endl;
            if (!shellWinrmBackend.empty()) std::wcout << L"  winrm-backend=" << shellWinrmBackend << std::endl;
            if (!shellFormat.empty()) std::wcout << L"  format=" << shellFormat << std::endl;
            if (!shellConsoleEncoding.empty()) std::wcout << L"  console-encoding=" << shellConsoleEncoding << std::endl;
            if (shellNoHeader) std::wcout << L"  no-header" << std::endl;
            continue;
        }

        if (command == L"set" && tokens.size() >= 2) {
            std::wstring key = tokens[1];
            std::transform(key.begin(), key.end(), key.begin(), towlower);
            std::wstring value;
            if (tokens.size() >= 3) {
                value = tokens[2];
            }

            if (key == L"node") {
                shellNode = value;
            } else if (key == L"user") {
                shellUser = value;
            } else if (key == L"password") {
                shellPassword = value;
            } else if (key == L"domain") {
                shellDomain = value;
            } else if (key == L"namespace") {
                shellNamespace = value;
            } else if (key == L"transport") {
                shellTransport = value;
            } else if (key == L"winrm-backend") {
                shellWinrmBackend = value;
            } else if (key == L"format") {
                shellFormat = value;
            } else if (key == L"console-encoding") {
                shellConsoleEncoding = value;
            } else if (key == L"no-header") {
                shellNoHeader = true;
            } else {
                std::wcout << L"Unknown shell setting: " << key << std::endl;
            }
            continue;
        }

        std::vector<std::wstring> childArgs;
        if (!shellNode.empty()) {
            childArgs.push_back(L"--node");
            childArgs.push_back(shellNode);
        }
        if (!shellUser.empty()) {
            childArgs.push_back(L"--user");
            childArgs.push_back(shellUser);
        }
        if (!shellPassword.empty()) {
            childArgs.push_back(L"--password");
            childArgs.push_back(shellPassword);
        }
        if (!shellDomain.empty()) {
            childArgs.push_back(L"--domain");
            childArgs.push_back(shellDomain);
        }
        if (!shellNamespace.empty()) {
            childArgs.push_back(L"--namespace");
            childArgs.push_back(shellNamespace);
        }
        if (!shellTransport.empty()) {
            childArgs.push_back(L"--transport");
            childArgs.push_back(shellTransport);
        }
        if (!shellWinrmBackend.empty()) {
            childArgs.push_back(L"--winrm-backend");
            childArgs.push_back(shellWinrmBackend);
        }
        if (!shellFormat.empty()) {
            childArgs.push_back(L"--format");
            childArgs.push_back(shellFormat);
        }
        if (!shellConsoleEncoding.empty()) {
            childArgs.push_back(L"--console-encoding");
            childArgs.push_back(shellConsoleEncoding);
        }
        if (shellNoHeader) {
            childArgs.push_back(L"--no-header");
        }

        childArgs.insert(childArgs.end(), tokens.begin(), tokens.end());

        int exitCode = EXIT_OK;
        std::wstring launchError;
        if (!RunChildProcessAndWait(executablePath, childArgs, launchError, exitCode)) {
            std::wcerr << L"[!] " << launchError << std::endl;
            return EXIT_WMI_FAILURE;
        }

        if (exitCode != EXIT_OK) {
            std::wcout << L"[shell] Command exited with code " << exitCode << std::endl;
        }
    }

    std::wcout << L"Leaving XMIC shell." << std::endl;
    return EXIT_OK;
}
