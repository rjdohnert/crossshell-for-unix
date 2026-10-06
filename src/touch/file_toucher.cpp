#include "file_toucher.hpp"
#include "touch_options.hpp"

bool FileToucher::Touch(const std::wstring& target, const TouchOptions& opts, const FILETIME& target_at, const FILETIME& target_mt) const {
        DWORD creation_disposition = opts.no_create ? OPEN_EXISTING : OPEN_ALWAYS;
        HANDLE hFile = CreateFileW(
            target.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            creation_disposition,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (opts.no_create && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) {
                return true;
            }
            std::wcerr << L"touch: " << target << L": cannot touch. Error: " << err << std::endl;
            return false;
        }

        const FILETIME* p_at = nullptr;
        const FILETIME* p_mt = nullptr;

        if (opts.change_access && !opts.change_mod) {
            p_at = &target_at;
        } else if (opts.change_mod && !opts.change_access) {
            p_mt = &target_mt;
        } else {
            p_at = &target_at;
            p_mt = &target_mt;
        }

        bool success = true;
        if (!SetFileTime(hFile, nullptr, p_at, p_mt)) {
            std::wcerr << L"touch: " << target << L": setting time failed. Error: " << GetLastError() << std::endl;
            success = false;
        }

        CloseHandle(hFile);
        return success;
    }
