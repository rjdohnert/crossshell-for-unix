#include "engine.hpp"

std::wstring PrintenvEngine::getEnvVar(const std::wstring& name, bool& exists) {
    exists = false;
    DWORD bufferSize = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (bufferSize == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            exists = false;
            return L"";
        }
        exists = true;
        return L"";
    }

    std::wstring val(bufferSize, L'\0');
    DWORD copied = GetEnvironmentVariableW(name.c_str(), &val[0], bufferSize);
    if (copied == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        exists = false;
        return L"";
    }

    if (copied >= bufferSize) {
        val.resize(copied);
        copied = GetEnvironmentVariableW(name.c_str(), &val[0], copied);
    }

    exists = true;
    if (copied > 0) {
        val.resize(copied);
    } else {
        val.clear();
    }
    return val;
}

int PrintenvEngine::execute(const PrintenvOptions& opts) {
    OutputFormatter::configureMode();

    // Case 1: Print all environment variables
    if (opts.targets.empty()) {
        LPWCH envBlock = GetEnvironmentStringsW();
        if (envBlock == nullptr) {
            std::wcerr << L"printenv: failed to retrieve environment block\n";
            return 1;
        }

        LPWCH current = envBlock;
        while (*current != L'\0') {
            std::wstring envStr(current);

            // Skip hidden Windows internal command variables
            if (!envStr.empty() && envStr[0] != L'=') {
                if (opts.nullTerminated) {
                    std::wcout << envStr;
                    std::wcout.put(L'\0');
                } else {
                    size_t eq = envStr.find(L'=');
                    if (eq != std::wstring::npos) {
                        std::wstring name = envStr.substr(0, eq);
                        std::wstring value = envStr.substr(eq + 1);
                        if (OutputFormatter::isPathVariable(name) && OutputFormatter::isConsoleFd(_fileno(stdout))) {
                            OutputFormatter::printPathColumns(name, value, true);
                            current += envStr.length() + 1;
                            continue;
                        }
                    }
                    std::wcout << OutputFormatter::sanitize(envStr);
                    std::wcout.put(L'\n');
                }
            }
            current += envStr.length() + 1;
        }

        FreeEnvironmentStringsW(envBlock);
        std::wcout.flush();
        return 0;
    }

    // Case 2: Specific environment variables requested
    bool allFound = true;
    for (const auto& target : opts.targets) {
        bool exists = false;
        std::wstring val = getEnvVar(target, exists);
        if (exists) {
            if (opts.nullTerminated) {
                std::wcout << val;
                std::wcout.put(L'\0');
            } else {
                if (OutputFormatter::isPathVariable(target) && OutputFormatter::isConsoleFd(_fileno(stdout))) {
                    OutputFormatter::printPathColumns(target, val, false);
                } else {
                    std::wcout << OutputFormatter::sanitize(val);
                    std::wcout.put(L'\n');
                }
            }
        } else {
            allFound = false;
        }
    }

    std::wcout.flush();
    return allFound ? 0 : 1;
}
