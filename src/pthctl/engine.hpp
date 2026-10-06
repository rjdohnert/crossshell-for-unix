#pragma once

#include "pthctl.hpp"

class PthctlEngine {
public:
    static std::wstring NormalizePath(const std::wstring& path);
    static bool PathEquals(const std::wstring& p1, const std::wstring& p2);
    static std::vector<std::wstring> SplitPath(const std::wstring& rawPath);
    static std::vector<std::wstring> SplitPathRaw(const std::wstring& rawPath);
    static std::wstring JoinPath(const std::vector<std::wstring>& list);
    static std::wstring JoinRawPath(const std::vector<std::wstring>& list);
    static bool ValidatePathLength(const std::wstring& updatedPath, bool dryRun);
    static bool ReadRegistryPath(Scope scope, std::wstring& outPath, DWORD& outType);
    static bool WriteRegistryPath(Scope scope, const std::wstring& newPath, DWORD type);

    static int CommandList(Scope scope);
    static int CommandAdd(Scope scope, const std::wstring& targetPath, bool prepend, bool dryRun);
    static int CommandRemove(Scope scope, const std::wstring& targetPath, bool dryRun);
    static int CommandCheck(Scope scope, const std::wstring& targetPath);
    static int CommandClean(Scope scope, bool dryRun);
};
