#pragma once

#include <filesystem>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class GzipTempFile {
private:
    std::filesystem::path m_path;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    bool m_committed = false;

    bool createInDirectory(const std::filesystem::path& directory,
                           std::wstring& error);

public:
    GzipTempFile() = default;
    ~GzipTempFile();

    GzipTempFile(const GzipTempFile&) = delete;
    GzipTempFile& operator=(const GzipTempFile&) = delete;

    bool createFor(const std::filesystem::path& target, std::wstring& error);
    bool createInSystemTemp(std::wstring& error);
    bool write(const char* data, size_t length, std::wstring& error);
    bool finish(std::wstring& error);
    const std::filesystem::path& path() const;
    bool commit(const std::filesystem::path& target, bool force,
                std::wstring& error);
    bool copyToStdout(std::wstring& error);
};

bool InspectGzipPath(const std::filesystem::path& path, bool& exists,
                     DWORD& attributes, std::wstring& error);
bool DeleteGzipInput(const std::filesystem::path& path, std::wstring& error);
