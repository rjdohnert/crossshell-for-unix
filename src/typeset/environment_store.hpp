#pragma once

#include "typeset.hpp"

class EnvironmentStore {
public:
    static std::map<std::wstring, std::wstring> GetProcessEnvironmentMap();

    static bool SetProcessEnvironmentVariable(const std::wstring& var, const std::wstring& val);
};
