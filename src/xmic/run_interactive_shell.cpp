#include "xmic_common.hpp"

static void SecureClearShellString(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(&value[0], value.size() * sizeof(wchar_t));
        value.clear();
    }
}

bool ParseInteractiveShellSetCommand(
    const std::vector<std::wstring>& tokens,
    std::wstring& keyOut,
    std::wstring& valueOut,
    bool& resetOut,
    std::wstring& errorOut
) {
    keyOut.clear();
    valueOut.clear();
    resetOut = false;
    errorOut.clear();

    if (tokens.size() < 2) {
        errorOut = L"Missing shell setting name.";
        return false;
    }
    if (tokens.size() < 3 || tokens[2].empty()) {
        errorOut = L"Missing value for shell setting. Use 'reset' to clear a setting.";
        return false;
    }
    if (tokens.size() != 3) {
        errorOut = L"Unexpected trailing values for shell setting.";
        return false;
    }

    keyOut = tokens[1];
    std::transform(keyOut.begin(), keyOut.end(), keyOut.begin(), towlower);
    const std::vector<std::wstring> knownSettings = {
        L"node", L"user", L"password", L"domain", L"namespace", L"transport",
        L"winrm-backend", L"format", L"console-encoding", L"no-header"
    };
    if (std::find(knownSettings.begin(), knownSettings.end(), keyOut) == knownSettings.end()) {
        errorOut = L"Unknown shell setting: " + keyOut;
        return false;
    }

    valueOut = tokens[2];
    std::wstring normalizedValue = valueOut;
    std::transform(normalizedValue.begin(), normalizedValue.end(), normalizedValue.begin(), towlower);
    if (normalizedValue == L"reset") {
        resetOut = true;
        valueOut.clear();
        if (keyOut == L"password") SecureClearShellString(normalizedValue);
        return true;
    }
    if (keyOut == L"no-header") {
        if (normalizedValue == L"true" || normalizedValue == L"on" || normalizedValue == L"yes" || normalizedValue == L"1") {
            valueOut = L"true";
        } else if (normalizedValue == L"false" || normalizedValue == L"off" || normalizedValue == L"no" || normalizedValue == L"0") {
            valueOut = L"false";
        } else {
            errorOut = L"Invalid value for no-header. Use true/false, on/off, yes/no, 1/0, or reset.";
            return false;
        }
    }
    if (keyOut == L"password") SecureClearShellString(normalizedValue);
    return true;
}

class ScopedShellPasswordEnvironment {
public:
    explicit ScopedShellPasswordEnvironment(const std::wstring& password)
        : name_(L"XMIC_INTERACTIVE_PASSWORD") {
        DWORD required = GetEnvironmentVariableW(name_.c_str(), nullptr, 0);
        if (required > 0) {
            std::vector<wchar_t> previousBuffer(required);
            DWORD copied = GetEnvironmentVariableW(name_.c_str(), previousBuffer.data(), required);
            if (copied > 0 && copied < required) {
                previousValue_.assign(previousBuffer.data(), copied);
                hadPreviousValue_ = true;
            } else if (copied >= required) {
                SecureZeroMemory(previousBuffer.data(), previousBuffer.size() * sizeof(wchar_t));
                return;
            }
            SecureZeroMemory(previousBuffer.data(), previousBuffer.size() * sizeof(wchar_t));
        }
        active_ = SetEnvironmentVariableW(name_.c_str(), password.c_str()) != FALSE;
    }

    ~ScopedShellPasswordEnvironment() {
        if (active_) {
            SetEnvironmentVariableW(name_.c_str(), hadPreviousValue_ ? previousValue_.c_str() : nullptr);
        }
        SecureClearShellString(previousValue_);
    }

    bool IsSet() const {
        return active_;
    }

private:
    std::wstring name_;
    std::wstring previousValue_;
    bool hadPreviousValue_ = false;
    bool active_ = false;
};

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
    struct PasswordScrubber {
        std::wstring* value;
        ~PasswordScrubber() {
            if (value != nullptr) SecureClearShellString(*value);
        }
    } passwordScrubber{ &shellPassword };

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
        SecureClearShellString(inputLine);
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
            std::wcout << L"                            Use 'reset' to clear a setting; no-header accepts true/false, on/off, yes/no, or 1/0" << std::endl;
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

        if (command == L"set") {
            std::wstring key;
            std::wstring value;
            std::wstring setError;
            bool reset = false;
            if (!ParseInteractiveShellSetCommand(tokens, key, value, reset, setError)) {
                std::wcout << setError << std::endl;
                if (tokens.size() >= 2) {
                    std::wstring attemptedKey = tokens[1];
                    std::transform(attemptedKey.begin(), attemptedKey.end(), attemptedKey.begin(), towlower);
                    if (attemptedKey == L"password") {
                        for (size_t i = 2; i < tokens.size(); ++i) SecureClearShellString(tokens[i]);
                    }
                }
                continue;
            }
            if (key == L"node") {
                shellNode = reset ? L"" : value;
            } else if (key == L"user") {
                shellUser = reset ? L"" : value;
            } else if (key == L"password") {
                SecureClearShellString(shellPassword);
                if (!reset) shellPassword = value;
            } else if (key == L"domain") {
                shellDomain = reset ? L"" : value;
            } else if (key == L"namespace") {
                shellNamespace = reset ? L"" : value;
            } else if (key == L"transport") {
                shellTransport = reset ? L"" : value;
            } else if (key == L"winrm-backend") {
                shellWinrmBackend = reset ? L"" : value;
            } else if (key == L"format") {
                shellFormat = reset ? L"" : value;
            } else if (key == L"console-encoding") {
                shellConsoleEncoding = reset ? L"" : value;
            } else if (key == L"no-header") {
                shellNoHeader = !reset && value == L"true";
            }
            SecureClearShellString(value);
            if (key == L"password" && tokens.size() >= 3) SecureClearShellString(tokens[2]);
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
        if (!shellPassword.empty()) {
            childArgs.push_back(L"--xmic-shell-password-env");
        }

        childArgs.insert(childArgs.end(), tokens.begin(), tokens.end());

        int exitCode = EXIT_OK;
        std::wstring launchError;
        bool launched = false;
        if (!shellPassword.empty()) {
            ScopedShellPasswordEnvironment passwordEnvironment(shellPassword);
            if (!passwordEnvironment.IsSet()) {
                std::wcerr << L"[!] Failed to stage the interactive shell password for the child process." << std::endl;
                return EXIT_WMI_FAILURE;
            }
            launched = RunChildProcessAndWait(executablePath, childArgs, launchError, exitCode);
        } else {
            launched = RunChildProcessAndWait(executablePath, childArgs, launchError, exitCode);
        }
        if (!launched) {
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
