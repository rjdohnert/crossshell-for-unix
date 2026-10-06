#include "command_classifier.hpp"

std::wstring CommandClassifier::toLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return s;
    }

std::vector<std::wstring> CommandClassifier::split(const std::wstring& str, wchar_t delimiter) {
        std::vector<std::wstring> tokens;
        std::wstring token;
        std::wistringstream ss(str);
        while (std::getline(ss, token, delimiter)) {
            if (!token.empty()) tokens.push_back(token);
        }
        return tokens;
    }

bool CommandClassifier::isBuiltin(const std::wstring& name) {
        std::wstring lower = toLower(name);
        return CMD_BUILTINS.find(lower) != CMD_BUILTINS.end();
    }

std::vector<std::wstring> CommandClassifier::findInPath(const std::wstring& name, bool showAll) {
        std::vector<std::wstring> matches;
        std::set<std::wstring> visited;

        DWORD pathLen = GetEnvironmentVariableW(L"PATH", nullptr, 0);
        if (pathLen == 0) return matches;
        std::wstring pathEnv(pathLen, L'\0');
        GetEnvironmentVariableW(L"PATH", &pathEnv[0], pathLen);

        std::vector<std::wstring> dirs = split(pathEnv, L';');
        dirs.insert(dirs.begin(), L".");

        DWORD extLen = GetEnvironmentVariableW(L"PATHEXT", nullptr, 0);
        std::wstring extEnv = L".COM;.EXE;.BAT;.CMD";
        if (extLen > 0) {
            extEnv.resize(extLen);
            GetEnvironmentVariableW(L"PATHEXT", &extEnv[0], extLen);
        }
        std::vector<std::wstring> exts = split(extEnv, L';');
        exts.insert(exts.begin(), L"");

        for (const auto& dir : dirs) {
            if (dir.empty()) continue;
            for (const auto& ext : exts) {
                fs::path candidate = fs::path(dir) / (name + ext);
                std::error_code ec;
                if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                    std::wstring full = fs::absolute(candidate, ec).wstring();
                    std::wstring lowerFull = toLower(full);
                    if (visited.find(lowerFull) == visited.end()) {
                        visited.insert(lowerFull);
                        matches.push_back(full);
                        if (!showAll) return matches;
                    }
                }
            }
        }
        return matches;
    }
