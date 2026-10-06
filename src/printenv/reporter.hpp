#pragma once

#include "printenv.hpp"

class OutputFormatter {
public:
    static bool isConsoleFd(int fd);
    static void configureMode();
    static std::wstring sanitize(const std::wstring& input);
    static bool iequals(const std::wstring& a, const std::wstring& b);
    static bool isPathVariable(const std::wstring& name);
    static std::vector<std::wstring> splitPath(const std::wstring& value);
    static void printPathColumns(const std::wstring& name, const std::wstring& value, bool includeName);
};
