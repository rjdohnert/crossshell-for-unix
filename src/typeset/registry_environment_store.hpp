#pragma once

#include "typeset.hpp"

class RegistryEnvironmentStore {
public:
    static bool SetPersistentUserEnv(const std::wstring& var, const std::wstring& val);
};
