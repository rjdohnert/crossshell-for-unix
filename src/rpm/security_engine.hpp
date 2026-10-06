#pragma once

#include "rpm.hpp"

class SecurityEngine {
public:
    static bool isAdministrator();

    static bool applyPosixPermissions(const fs::path& filePath, uint32_t posixMode);
};
