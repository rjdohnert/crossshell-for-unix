#include "file_system_inspector.hpp"

bool FileSystemInspector::FileExists(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES);
    }

bool FileSystemInspector::IsDirectory(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

bool FileSystemInspector::IsRegularFile(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

bool FileSystemInspector::IsSymlink(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_REPARSE_POINT);
    }

bool FileSystemInspector::IsNonEmpty(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
            uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            return size > 0;
        }
        return false;
    }

bool FileSystemInspector::IsReadable(const std::wstring& path) {
        return (_waccess(path.c_str(), 4) == 0);
    }

bool FileSystemInspector::IsWritable(const std::wstring& path) {
        return (_waccess(path.c_str(), 2) == 0);
    }

bool FileSystemInspector::IsExecutable(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        if (attr & FILE_ATTRIBUTE_DIRECTORY) return true;

        size_t dot = path.find_last_of(L".");
        if (dot != std::wstring::npos) {
            std::wstring ext = path.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".exe" || ext == L".cmd" || ext == L".bat" || ext == L".com" ||
                ext == L".ps1" || ext == L".vbs" || ext == L".js" || ext == L".msc") {
                return true;
            }
        }
        return (_waccess(path.c_str(), 4) == 0);
    }

bool FileSystemInspector::IsTerminal(const std::wstring& fd_str) {
        try {
            int fd = std::stoi(fd_str);
            DWORD handle_id = (fd == 0) ? STD_INPUT_HANDLE :
                              (fd == 1) ? STD_OUTPUT_HANDLE :
                              (fd == 2) ? STD_ERROR_HANDLE : 0;
            if (handle_id == 0) return false;

            HANDLE h = GetStdHandle(handle_id);
            if (h == INVALID_HANDLE_VALUE || h == NULL) return false;
            DWORD mode;
            return GetConsoleMode(h, &mode) != 0;
        } catch (...) {
            return false;
        }
    }

bool FileSystemInspector::IsCharDevice(const std::wstring& path) {
        std::wstring u = path;
        std::transform(u.begin(), u.end(), u.begin(), ::towupper);
        if (u == L"CON" || u == L"NUL" || u == L"PRN" || u == L"AUX" ||
            u.rfind(L"COM", 0) == 0 || u.rfind(L"LPT", 0) == 0) {
            return true;
        }
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD type = GetFileType(h);
            CloseHandle(h);
            return type == FILE_TYPE_CHAR;
        }
        return false;
    }

bool FileSystemInspector::IsNamedPipe(const std::wstring& path) {
        if (path.rfind(L"\\\\.\\pipe\\", 0) == 0) return true;
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD type = GetFileType(h);
            CloseHandle(h);
            return type == FILE_TYPE_PIPE;
        }
        return false;
    }

bool FileSystemInspector::FileNewerThan(const std::wstring& p1, const std::wstring& p2) {
        WIN32_FILE_ATTRIBUTE_DATA d1, d2;
        if (!GetFileAttributesExW(p1.c_str(), GetFileExInfoStandard, &d1)) return false;
        if (!GetFileAttributesExW(p2.c_str(), GetFileExInfoStandard, &d2)) return false;

        ULARGE_INTEGER t1, t2;
        t1.LowPart = d1.ftLastWriteTime.dwLowDateTime;
        t1.HighPart = d1.ftLastWriteTime.dwHighDateTime;
        t2.LowPart = d2.ftLastWriteTime.dwLowDateTime;
        t2.HighPart = d2.ftLastWriteTime.dwHighDateTime;

        return t1.QuadPart > t2.QuadPart;
    }

bool FileSystemInspector::FileOlderThan(const std::wstring& p1, const std::wstring& p2) {
        WIN32_FILE_ATTRIBUTE_DATA d1, d2;
        if (!GetFileAttributesExW(p1.c_str(), GetFileExInfoStandard, &d1)) return false;
        if (!GetFileAttributesExW(p2.c_str(), GetFileExInfoStandard, &d2)) return false;

        ULARGE_INTEGER t1, t2;
        t1.LowPart = d1.ftLastWriteTime.dwLowDateTime;
        t1.HighPart = d1.ftLastWriteTime.dwHighDateTime;
        t2.LowPart = d2.ftLastWriteTime.dwLowDateTime;
        t2.HighPart = d2.ftLastWriteTime.dwHighDateTime;

        return t1.QuadPart < t2.QuadPart;
    }

bool FileSystemInspector::SameFile(const std::wstring& p1, const std::wstring& p2) {
        HANDLE h1 = CreateFileW(p1.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h1 == INVALID_HANDLE_VALUE) return false;

        HANDLE h2 = CreateFileW(p2.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h2 == INVALID_HANDLE_VALUE) {
            CloseHandle(h1);
            return false;
        }

        BY_HANDLE_FILE_INFORMATION i1, i2;
        bool res = false;
        if (GetFileInformationByHandle(h1, &i1) && GetFileInformationByHandle(h2, &i2)) {
            res = (i1.dwVolumeSerialNumber == i2.dwVolumeSerialNumber) &&
                  (i1.nFileIndexHigh == i2.nFileIndexHigh) &&
                  (i1.nFileIndexLow == i2.nFileIndexLow);
        }

        CloseHandle(h1);
        CloseHandle(h2);
        return res;
    }
