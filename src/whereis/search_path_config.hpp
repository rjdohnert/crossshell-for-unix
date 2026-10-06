#pragma once

#include "whereis.hpp"

class SearchPathConfig {
public:
    bool searchBin{true};
    bool searchMan{true};
    bool searchSrc{true};

    std::vector<std::wstring> binDirs;
    std::vector<std::wstring> manDirs;
    std::vector<std::wstring> srcDirs;

    std::vector<std::wstring> binExts;
    std::vector<std::wstring> manExts = { L"", L".chm", L".hlp", L".html", L".htm", L".pdf", L".txt", L".md", L".1" };
    std::vector<std::wstring> srcExts = { L"", L".cpp", L".c", L".h", L".hpp", L".cs", L".py", L".rs", L".go", L".java", L".js", L".ts" };

    static std::wstring getEnvVar(const wchar_t* varName);

    static std::vector<std::wstring> split(const std::wstring& str, wchar_t delim);

    void initializeDefaults();
};
