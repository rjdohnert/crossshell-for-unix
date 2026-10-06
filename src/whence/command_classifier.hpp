#pragma once

#include "whence.hpp"

class CommandClassifier {
private:
    static inline const std::set<std::wstring> CMD_BUILTINS = {
        L"assoc", L"break", L"call", L"cd", L"chdir", L"cls", L"color", L"copy",
        L"date", L"del", L"dir", L"echo", L"endlocal", L"erase", L"exit", L"for",
        L"ftype", L"goto", L"if", L"md", L"mkdir", L"mklink", L"move", L"path",
        L"pause", L"prompt", L"rd", L"ren", L"rename", L"rmdir", L"set", L"setlocal",
        L"shift", L"start", L"time", L"title", L"type", L"ver", L"verify", L"vol"
    };

    static std::wstring toLower(std::wstring s);

    static std::vector<std::wstring> split(const std::wstring& str, wchar_t delimiter);

public:
    static bool isBuiltin(const std::wstring& name);

    static std::vector<std::wstring> findInPath(const std::wstring& name, bool showAll);
};
