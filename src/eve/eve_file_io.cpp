#define NOMINMAX
#include <windows.h>

#include "eve_file_io.h"

#include <atomic>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace {

std::atomic<unsigned long long> unique_path_counter{0};

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    HANDLE get() const noexcept { return value_; }
    void reset(HANDLE value = nullptr) noexcept {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
        value_ = value;
    }
    HANDLE release() noexcept {
        HANDLE value = value_;
        value_ = nullptr;
        return value;
    }

private:
    HANDLE value_;
};

void SetError(std::string& error, const char* action, DWORD code) {
    error = action;
    error += " (Windows error ";
    error += std::to_string(code);
    error += ")";
}

std::filesystem::path UniqueSibling(const std::filesystem::path& path, const wchar_t* suffix) {
    std::wstring name = path.filename().native();
    name += suffix;
    name += std::to_wstring(GetCurrentProcessId());
    name += L".";
    name += std::to_wstring(unique_path_counter.fetch_add(1, std::memory_order_relaxed));
    return path.parent_path() / name;
}

bool WriteTemporaryFile(const std::filesystem::path& path, std::string_view contents, std::string& error, DWORD& failure_code) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) {
        failure_code = GetLastError();
        SetError(error, "unable to create a unique temporary file", failure_code);
        return false;
    }

    size_t offset = 0;
    while (offset < contents.size()) {
        const size_t remaining = contents.size() - offset;
        const DWORD chunk_size = remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
        DWORD written = 0;
        if (!WriteFile(file.get(), contents.data() + offset, chunk_size, &written, nullptr)) {
            failure_code = GetLastError();
            file.reset();
            DeleteFileW(path.c_str());
            SetError(error, "unable to write temporary file", failure_code);
            return false;
        }
        if (written == 0) {
            failure_code = ERROR_WRITE_FAULT;
            file.reset();
            DeleteFileW(path.c_str());
            SetError(error, "temporary file write made no progress", failure_code);
            return false;
        }
        offset += written;
    }
    if (!FlushFileBuffers(file.get())) {
        failure_code = GetLastError();
        file.reset();
        DeleteFileW(path.c_str());
        SetError(error, "unable to flush temporary file", failure_code);
        return false;
    }
    failure_code = ERROR_SUCCESS;
    return true;
}

bool MoveExistingBackup(const std::filesystem::path& backup, std::filesystem::path& saved_backup, std::string& error) {
    const DWORD attributes = GetFileAttributesW(backup.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        SetError(error, "unable to inspect existing backup", code);
        return false;
    }

    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        saved_backup = UniqueSibling(backup, L".previous.");
        if (MoveFileExW(backup.c_str(), saved_backup.c_str(), MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        const DWORD code = GetLastError();
        if (code != ERROR_ALREADY_EXISTS && code != ERROR_FILE_EXISTS) {
            SetError(error, "unable to preserve existing backup", code);
            return false;
        }
    }
    SetError(error, "unable to allocate a unique backup path", ERROR_FILE_EXISTS);
    return false;
}

}

FileReadResult ReadFileBytes(const std::filesystem::path& path, std::string& contents, std::string& error) {
    contents.clear();
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
            return FileReadResult::Missing;
        }
        SetError(error, "unable to open file", code);
        return FileReadResult::Error;
    }

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(file.get(), &file_size)) {
        SetError(error, "unable to determine file size", GetLastError());
        return FileReadResult::Error;
    }
    if (file_size.QuadPart < 0 ||
        static_cast<unsigned long long>(file_size.QuadPart) > std::string().max_size()) {
        SetError(error, "file size is invalid or too large", ERROR_FILE_TOO_LARGE);
        return FileReadResult::Error;
    }

    try {
        contents.reserve(static_cast<size_t>(file_size.QuadPart));
        char buffer[64 * 1024];
        for (;;) {
            DWORD bytes_read = 0;
            if (!ReadFile(file.get(), buffer, sizeof(buffer), &bytes_read, nullptr)) {
                contents.clear();
                SetError(error, "unable to read file", GetLastError());
                return FileReadResult::Error;
            }
            if (bytes_read == 0) {
                break;
            }
            contents.append(buffer, bytes_read);
        }
    } catch (const std::bad_alloc&) {
        contents.clear();
        error = "not enough memory to read file";
        return FileReadResult::Error;
    }
    return FileReadResult::Loaded;
}

bool WriteFileAtomically(const std::filesystem::path& path, std::string_view contents, bool keep_backup, std::string& error) {
    if (path.empty() || path.filename().empty()) {
        error = "target file path is empty";
        return false;
    }

    std::filesystem::path temporary;
    bool temporary_created = false;
    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        temporary = UniqueSibling(path, L".eve.tmp.");
        std::string create_error;
        DWORD create_code = ERROR_SUCCESS;
        if (WriteTemporaryFile(temporary, contents, create_error, create_code)) {
            temporary_created = true;
            break;
        }
        if (create_code != ERROR_ALREADY_EXISTS && create_code != ERROR_FILE_EXISTS) {
            error = std::move(create_error);
            return false;
        }
    }
    if (!temporary_created) {
        SetError(error, "unable to allocate a unique temporary path", ERROR_FILE_EXISTS);
        return false;
    }

    const DWORD target_attributes = GetFileAttributesW(path.c_str());
    const DWORD target_error = target_attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    if (target_attributes != INVALID_FILE_ATTRIBUTES && (target_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        DeleteFileW(temporary.c_str());
        error = "target path is a directory";
        return false;
    }
    if (target_attributes == INVALID_FILE_ATTRIBUTES &&
        target_error != ERROR_FILE_NOT_FOUND && target_error != ERROR_PATH_NOT_FOUND) {
        DeleteFileW(temporary.c_str());
        SetError(error, "unable to inspect target file", target_error);
        return false;
    }

    const bool target_exists = target_attributes != INVALID_FILE_ATTRIBUTES;
    std::filesystem::path backup;
    std::filesystem::path previous_backup;
    if (keep_backup && target_exists) {
        backup = path;
        backup += L".bak";
        if (!MoveExistingBackup(backup, previous_backup, error)) {
            DeleteFileW(temporary.c_str());
            return false;
        }
    }

    BOOL replaced = FALSE;
    DWORD replace_error = ERROR_SUCCESS;
    if (target_exists) {
        replaced = ReplaceFileW(path.c_str(), temporary.c_str(),
            keep_backup ? backup.c_str() : nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr);
        if (!replaced) {
            replace_error = GetLastError();
        }
    } else {
        replaced = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
        if (!replaced) {
            replace_error = GetLastError();
        }
    }

    if (!replaced) {
        DeleteFileW(temporary.c_str());
        if (!previous_backup.empty() &&
            !MoveFileExW(previous_backup.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD restore_error = GetLastError();
            SetError(error, "file replacement failed and existing backup restoration also failed", restore_error);
            error += "; replacement error ";
            error += std::to_string(replace_error);
            return false;
        }
        SetError(error, "unable to replace target file", replace_error);
        return false;
    }

    if (!previous_backup.empty()) {
        DeleteFileW(previous_backup.c_str());
    }
    return true;
}
