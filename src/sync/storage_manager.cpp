#include "storage_manager.hpp"
#include "volume_target.hpp"

std::vector<std::unique_ptr<VolumeTarget>> StorageManager::discover_volumes(bool removable_only) {
        std::vector<std::unique_ptr<VolumeTarget>> volumes;
        DWORD drivesMask = GetLogicalDrives();

        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if (drivesMask & (1 << (letter - 'A'))) {
                std::wstring root = std::wstring(1, letter) + L":\\";
                UINT driveType = GetDriveTypeW(root.c_str());

                // Skip non-local/non-disk drives
                if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_REMOTE) {
                    continue;
                }

                if (removable_only && driveType != DRIVE_REMOVABLE) {
                    continue;
                }

                volumes.push_back(std::make_unique<VolumeTarget>(letter, driveType));
            }
        }
        return volumes;
    }
