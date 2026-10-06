#include "file_truncator.hpp"
#include "size_calculator.hpp"

void FileTruncator::set_no_create(bool flag) { no_create = flag; }

void FileTruncator::set_size_calculator(SizeCalculator calc) {
        size_calc = std::make_unique<SizeCalculator>(calc);
    }

void FileTruncator::set_reference_file(std::wstring path) {
        ref_file = std::move(path);
    }

bool FileTruncator::truncate_file(const std::wstring& wpath, std::string& err_msg) {

        // 1. Resolve Target Size
        uint64_t target_size = 0;
        bool has_target_size = false;

        if (!ref_file.empty()) {
            uint64_t r_size = 0;
            if (!get_file_size(ref_file, r_size, err_msg)) {
                err_msg = "reference file error: " + err_msg;
                return false;
            }
            target_size = r_size;
            has_target_size = true;
        }

        // 2. Open or Create File
        DWORD creation_disposition = no_create ? OPEN_EXISTING : OPEN_ALWAYS;
        HANDLE hFile = CreateFileW(
            wpath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            creation_disposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD dwErr = GetLastError();
            // In authentic BSD truncate, -c skips non-existent files silently without error
            if (no_create && dwErr == ERROR_FILE_NOT_FOUND) {
                return true;
            }
            err_msg = win32_error_to_string(dwErr);
            return false;
        }

        // 3. Compute Size
        if (!has_target_size && size_calc) {
            LARGE_INTEGER cur_size;
            if (!GetFileSizeEx(hFile, &cur_size)) {
                err_msg = win32_error_to_string(GetLastError());
                CloseHandle(hFile);
                return false;
            }
            target_size = size_calc->compute_new_size(static_cast<uint64_t>(cur_size.QuadPart));
        }

        // 4. Set End Of File via Win32 API
        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(target_size);

        if (!SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN)) {
            err_msg = win32_error_to_string(GetLastError());
            CloseHandle(hFile);
            return false;
        }

        if (!SetEndOfFile(hFile)) {
            err_msg = win32_error_to_string(GetLastError());
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }

bool FileTruncator::get_file_size(const std::wstring& path, uint64_t& out_size, std::string& err) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
            err = win32_error_to_string(GetLastError());
            return false;
        }
        LARGE_INTEGER li;
        li.LowPart = data.nFileSizeLow;
        li.HighPart = data.nFileSizeHigh;
        out_size = static_cast<uint64_t>(li.QuadPart);
        return true;
    }

std::string FileTruncator::win32_error_to_string(DWORD err) {
        char* buffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer, 0, NULL
        );
        std::string message(buffer, size);
        LocalFree(buffer);
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == '.')) {
            message.pop_back();
        }
        return message.empty() ? ("Error " + std::to_string(err)) : message;
    }
