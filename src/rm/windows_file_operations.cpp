#include "windows_file_operations.hpp"

bool WindowsFileOperations::IsRootPath(const fs::path& p) {
        fs::path abs = fs::absolute(p);
        return (abs == abs.root_path());
    }

void WindowsFileOperations::StripReadOnly(const fs::path& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
            SetFileAttributesW(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    }

bool WindowsFileOperations::IsWriteProtected(const fs::path& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY));
    }
