#pragma once

#include "rm.hpp"

class WindowsFileOperations {
public:
    static bool IsRootPath(const fs::path& p);

    static void StripReadOnly(const fs::path& path);

    static bool IsWriteProtected(const fs::path& path);
};
