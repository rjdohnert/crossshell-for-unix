#pragma once

#include "pwd.hpp"
#include "options.hpp"

class PwdEngine {
public:
    static std::wstring stripExtendedPrefix(const std::wstring& path);
    static std::wstring getLogicalCwd();
    static std::wstring getPhysicalCwd(const std::wstring& logicalPath);
    static int execute(const PwdOptions& opts);
};
