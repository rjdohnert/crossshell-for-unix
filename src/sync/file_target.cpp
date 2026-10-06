#include "file_target.hpp"
#include "volume_target.hpp"

FileTarget::FileTarget(std::string path, bool file_system) 
        : path_str(std::move(path)), sync_containing_filesystem(file_system) {
        int len = MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, NULL, 0);
        path_wstr.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, &path_wstr[0], len);
    }

std::string FileTarget::get_name() const  {
        return path_str;
    }

std::string FileTarget::get_description() const  {
        return sync_containing_filesystem ? "Containing File System" : "File Data Stream";
    }

bool FileTarget::flush(std::string& error_msg)  {
        if (sync_containing_filesystem) {
            // Find root volume path of this file/folder
            wchar_t volume_path[MAX_PATH + 1] = {0};
            if (!GetVolumePathNameW(path_wstr.c_str(), volume_path, MAX_PATH)) {
                error_msg = "Cannot resolve parent volume for path";
                return false;
            }
            if (wcslen(volume_path) >= 2 && volume_path[1] == L':') {
                VolumeTarget parent_vol(volume_path[0], GetDriveTypeW(volume_path));
                return parent_vol.flush(error_msg);
            }
            error_msg = "Unsupported volume path format";
            return false;
        }

        // Open individual file handle to flush its specific data buffers
        HANDLE hFile = CreateFileW(
            path_wstr.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            error_msg = (err == ERROR_ACCESS_DENIED) ? "Access denied" : "Win32 Error " + std::to_string(err);
            return false;
        }

        BOOL success = FlushFileBuffers(hFile);
        if (!success) {
            DWORD err = GetLastError();
            error_msg = "FlushFileBuffers failed: Win32 Error " + std::to_string(err);
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }
