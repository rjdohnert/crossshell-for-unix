#pragma once

#include "test.hpp"

class FileSystemInspector {
public:
    static bool FileExists(const std::wstring& path);

    static bool IsDirectory(const std::wstring& path);

    static bool IsRegularFile(const std::wstring& path);

    static bool IsSymlink(const std::wstring& path);

    static bool IsNonEmpty(const std::wstring& path);

    static bool IsReadable(const std::wstring& path);

    static bool IsWritable(const std::wstring& path);

    static bool IsExecutable(const std::wstring& path);

    static bool IsTerminal(const std::wstring& fd_str);

    static bool IsCharDevice(const std::wstring& path);

    static bool IsNamedPipe(const std::wstring& path);

    static bool FileNewerThan(const std::wstring& p1, const std::wstring& p2);

    static bool FileOlderThan(const std::wstring& p1, const std::wstring& p2);

    static bool SameFile(const std::wstring& p1, const std::wstring& p2);
};
