#define WIN32_LEAN_AND_MEAN
#include "gzip_file.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace {
std::atomic<unsigned long long> g_tempCounter{0};

std::wstring SystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length != 0 && buffer != nullptr
        ? std::wstring(buffer, length)
        : L"Windows error " + std::to_wstring(code);
    if (buffer != nullptr) LocalFree(buffer);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

bool IsMissingPathError(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}
}

bool InspectGzipPath(const std::filesystem::path& path, bool& exists,
                     DWORD& attributes, std::wstring& error) {
    attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        exists = true;
        return true;
    }

    const DWORD code = GetLastError();
    if (IsMissingPathError(code)) {
        exists = false;
        return true;
    }
    error = L"cannot inspect " + path.native() + L": " + SystemMessage(code);
    return false;
}

bool DeleteGzipInput(const std::filesystem::path& path, std::wstring& error) {
    if (DeleteFileW(path.c_str())) return true;
    error = L"cannot remove input " + path.native() + L": " +
            SystemMessage(GetLastError());
    return false;
}

GzipTempFile::~GzipTempFile() {
    if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);
    if (!m_committed && !m_path.empty()) DeleteFileW(m_path.c_str());
}

bool GzipTempFile::createInDirectory(const std::filesystem::path& directory,
                                    std::wstring& error) {
    for (unsigned int attempt = 0; attempt < 128; ++attempt) {
        const auto id = g_tempCounter.fetch_add(1, std::memory_order_relaxed);
        const std::wstring name = L".gzip-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(id) +
            L".tmp";
        const std::filesystem::path candidate = directory / name;
        HANDLE handle = CreateFileW(candidate.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            m_path = candidate;
            m_handle = handle;
            return true;
        }

        const DWORD code = GetLastError();
        if (code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS) {
            error = L"cannot create temporary file in " + directory.native() +
                    L": " + SystemMessage(code);
            return false;
        }
    }
    error = L"could not allocate a unique temporary file";
    return false;
}

bool GzipTempFile::createFor(const std::filesystem::path& target,
                             std::wstring& error) {
    std::filesystem::path directory = target.parent_path();
    if (directory.empty()) directory = L".";
    return createInDirectory(directory, error);
}

bool GzipTempFile::createInSystemTemp(std::wstring& error) {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length >= buffer.size()) {
        error = L"cannot locate the system temporary directory: " +
                SystemMessage(GetLastError());
        return false;
    }
    return createInDirectory(std::filesystem::path(std::wstring(buffer.data(), length)),
                             error);
}

bool GzipTempFile::write(const char* data, size_t length, std::wstring& error) {
    size_t writtenTotal = 0;
    while (writtenTotal < length) {
        const DWORD request = static_cast<DWORD>(
            (std::min)(length - writtenTotal,
                       static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(m_handle, data + writtenTotal, request, &written, nullptr) ||
            written == 0) {
            error = L"cannot write temporary file: " + SystemMessage(GetLastError());
            return false;
        }
        writtenTotal += written;
    }
    return true;
}

const std::filesystem::path& GzipTempFile::path() const {
    return m_path;
}

bool GzipTempFile::finish(std::wstring& error) {
    bool succeeded = true;
    if (m_handle != INVALID_HANDLE_VALUE) {
        if (!FlushFileBuffers(m_handle)) {
            error = L"cannot flush temporary file: " + SystemMessage(GetLastError());
            succeeded = false;
        }
        if (!CloseHandle(m_handle)) {
            if (succeeded) {
                error = L"cannot close temporary file: " + SystemMessage(GetLastError());
            }
            succeeded = false;
        }
        m_handle = INVALID_HANDLE_VALUE;
    }
    return succeeded;
}

bool GzipTempFile::commit(const std::filesystem::path& target, bool force,
                          std::wstring& error) {
    if (!finish(error)) return false;

    bool targetExists = false;
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    if (!InspectGzipPath(target, targetExists, attributes, error)) return false;
    if (targetExists) {
        if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE |
                           FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
            error = L"refusing to replace a directory, device, or reparse point: " +
                    target.native();
            return false;
        }
        if (!force) {
            error = L"output already exists: " + target.native() +
                    L" (use -f to replace it)";
            return false;
        }
        if (!ReplaceFileW(target.c_str(), m_path.c_str(), nullptr,
                          REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            error = L"cannot atomically replace output " + target.native() +
                    L": " + SystemMessage(GetLastError());
            return false;
        }
    } else if (!MoveFileExW(m_path.c_str(), target.c_str(),
                            MOVEFILE_WRITE_THROUGH)) {
        error = L"cannot atomically install output " + target.native() +
                L": " + SystemMessage(GetLastError());
        return false;
    }

    m_committed = true;
    m_path.clear();
    return true;
}

bool GzipTempFile::copyToStdout(std::wstring& error) {
    if (!finish(error)) return false;
    std::ifstream input(m_path, std::ios::binary);
    if (!input) {
        error = L"cannot read temporary output: " + m_path.native();
        return false;
    }

    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            std::cout.write(buffer.data(), count);
            if (!std::cout) {
                error = L"failed while writing output to standard output";
                return false;
            }
        }
    }
    if (input.bad()) {
        error = L"failed while reading temporary output";
        return false;
    }
    std::cout.flush();
    if (!std::cout) {
        error = L"failed while flushing standard output";
        return false;
    }
    return true;
}
