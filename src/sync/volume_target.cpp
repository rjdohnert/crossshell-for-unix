#include "volume_target.hpp"

VolumeTarget::VolumeTarget(wchar_t letter, UINT type) : drive_type(type) {
        drive_letter = std::string(1, static_cast<char>(letter)) + ":";
        root_path = std::wstring(1, letter) + L":\\";
        device_path = L"\\\\.\\" + std::wstring(1, letter) + L":";

        wchar_t vol_name[MAX_PATH + 1] = {0};
        wchar_t file_system[MAX_PATH + 1] = {0};
        if (GetVolumeInformationW(root_path.c_str(), vol_name, MAX_PATH, NULL, NULL, NULL, file_system, MAX_PATH)) {
            char mb_vol[MAX_PATH * 2] = {0};
            char mb_fs[MAX_PATH * 2] = {0};
            WideCharToMultiByte(CP_UTF8, 0, vol_name, -1, mb_vol, sizeof(mb_vol), NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, file_system, -1, mb_fs, sizeof(mb_fs), NULL, NULL);
            volume_label = mb_vol;
            fs_name = mb_fs;
        }
    }

std::string VolumeTarget::get_name() const  {
        return drive_letter;
    }

std::string VolumeTarget::get_description() const  {
        std::string type_str = "Fixed";
        if (drive_type == DRIVE_REMOVABLE) type_str = "Removable";
        else if (drive_type == DRIVE_CDROM) type_str = "Optical";
        else if (drive_type == DRIVE_REMOTE) type_str = "Network";
        else if (drive_type == DRIVE_RAMDISK) type_str = "RAMDisk";

        std::string desc = type_str;
        if (!fs_name.empty()) desc += ", " + fs_name;
        if (!volume_label.empty()) desc += " [" + volume_label + "]";
        return desc;
    }

UINT VolumeTarget::get_drive_type() const { return drive_type; }

bool VolumeTarget::flush(std::string& error_msg)  {
        // Opening a volume handle with GENERIC_READ | GENERIC_WRITE requires admin rights
        HANDLE hVolume = CreateFileW(
            device_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hVolume == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                error_msg = "Access denied (requires Administrator privileges)";
            } else if (err == ERROR_NOT_READY) {
                error_msg = "Device not ready";
            } else {
                error_msg = "Win32 Error " + std::to_string(err);
            }
            return false;
        }

        BOOL success = FlushFileBuffers(hVolume);
        if (!success) {
            DWORD err = GetLastError();
            error_msg = "FlushFileBuffers failed: Win32 Error " + std::to_string(err);
            CloseHandle(hVolume);
            return false;
        }

        CloseHandle(hVolume);
        return true;
    }
