class VolumeManager {
public:
    static void ListDisksAndVolumes() {
        std::wcout << L"================ DISKS & VOLUMES ================\n\n";
        WCHAR volumeName[MAX_PATH];
        ScopedFindVolumeHandle hVolume(FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName)));

        if (!hVolume.IsValid()) {
            std::wcout << L"Failed to enumerate volumes. Error: " << GetLastError() << L"\n";
            return;
        }

        do {
            std::wcout << L"Volume Path: " << volumeName << L"\n";

            // Query Mount Points
            WCHAR pathNames[MAX_PATH] = { 0 };
            DWORD charCount = 0;
            if (GetVolumePathNamesForVolumeNameW(volumeName, pathNames, MAX_PATH, &charCount) && pathNames[0] != L'\0') {
                std::wcout << L"  Mount Points: ";
                for (PWCHAR p = pathNames; *p != L'\0'; p += wcslen(p) + 1) {
                    std::wcout << p << L" ";
                }
                std::wcout << L"\n";
            } else {
                std::wcout << L"  Mount Points: [Unmounted / No Drive Letter]\n";
            }

            // Query Filesystem & Label
            WCHAR label[MAX_PATH] = { 0 };
            WCHAR fsName[MAX_PATH] = { 0 };
            if (GetVolumeInformationW(volumeName, label, MAX_PATH, NULL, NULL, NULL, fsName, MAX_PATH)) {
                std::wcout << L"  Volume Label: " << (label[0] ? label : L"[No Label]") << L"\n";
                std::wcout << L"  File System : " << fsName << L"\n";
            }

            // Query Size
            ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
            if (GetDiskFreeSpaceExW(volumeName, &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
                double sizeGB = (double)totalBytes.QuadPart / (1024 * 1024 * 1024);
                double freeGB = (double)totalFreeBytes.QuadPart / (1024 * 1024 * 1024);
                std::wcout << L"  Total Size  : " << std::fixed << std::setprecision(2) << sizeGB << L" GB\n";
                std::wcout << L"  Free Space  : " << std::fixed << std::setprecision(2) << freeGB << L" GB\n";
            }
            std::wcout << L"-------------------------------------------------\n";
        } while (FindNextVolumeW(hVolume.Get(), volumeName, ARRAYSIZE(volumeName)));
    }

    static bool MountVolume(const std::wstring& driveLetter, const std::wstring& volumeGuid) {
        std::wstring path = driveLetter;
        if (path.back() != L'\\') path += L'\\';

        if (SetVolumeMountPointW(path.c_str(), volumeGuid.c_str())) {
            std::wcout << L"Successfully mounted " << volumeGuid << L" to " << path << L"\n";
            return true;
        } else {
            DWORD err = GetLastError();
            std::wcout << L"Failed to mount volume. Error " << err << L": " << SecurityInspector::GetErrorMessage(err) << L"\n";
            return false;
        }
    }

    static bool UnmountVolume(const std::wstring& driveLetter) {
        std::wstring path = driveLetter;
        if (path.back() != L'\\') path += L'\\';

        if (DeleteVolumeMountPointW(path.c_str())) {
            std::wcout << L"Successfully unmounted drive " << path << L"\n";
            return true;
        } else {
            DWORD err = GetLastError();
            std::wcout << L"Failed to unmount drive. Error " << err << L": " << SecurityInspector::GetErrorMessage(err) << L"\n";
            return false;
        }
    }

    static bool SetLabel(const std::wstring& driveLetter, const std::wstring& newLabel) {
        std::wstring path = driveLetter;
        if (path.back() != L'\\') path += L'\\';

        if (SetVolumeLabelW(path.c_str(), newLabel.c_str())) {
            std::wcout << L"Successfully set volume label of " << path << L" to \"" << newLabel << L"\"\n";
            return true;
        } else {
            DWORD err = GetLastError();
            std::wcout << L"Failed to set label. Error " << err << L": " << SecurityInspector::GetErrorMessage(err) << L"\n";
            return false;
        }
    }

    static bool FormatVolume(const std::wstring& driveLetter, std::wstring fsType, std::wstring label, bool quick) {
        ScopedLibraryHandle hFmifs(LoadLibraryW(L"fmifs.dll"));
        if (!hFmifs.IsValid()) {
            std::wcout << L"Error: Failed to load fmifs.dll\n";
            return false;
        }

        PFORMATEX FormatEx = (PFORMATEX)GetProcAddress(hFmifs.Get(), "FormatEx");
        if (!FormatEx) {
            std::wcout << L"Error: Unable to locate FormatEx API.\n";
            return false;
        }

        std::wstring path = driveLetter;
        if (path.back() != L'\\') path += L'\\';

        std::wcout << L"Formatting " << path << L" as " << fsType << L" (Label: " << label << L")...\n";

        BOOLEAN success = FormatEx((PWCHAR)path.c_str(), 0x0C, (PWCHAR)fsType.c_str(),
                                   (PWCHAR)label.c_str(), quick ? TRUE : FALSE, 0, FormatCallback);

        std::wcout << L"\n";

        if (success) {
            std::wcout << L"Format completed successfully.\n";
            return true;
        } else {
            std::wcout << L"Format operation failed.\n";
            return false;
        }
    }

    static bool SecureFormatVolume(const std::wstring& driveLetter, int passes) {
        std::wstring devicePath = L"\\\\.\\" + driveLetter;
        if (devicePath.back() == L'\\') devicePath.pop_back();

        ScopedFileHandle hDevice(CreateFileW(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));

        if (!hDevice.IsValid()) {
            DWORD err = GetLastError();
            std::wcout << L"Error opening volume for direct write. Error " << err << L": " << SecurityInspector::GetErrorMessage(err) << L"\n";
            return false;
        }

        // Lock & Dismount Volume
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(hDevice.Get(), FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
            std::wcout << L"Warning: Could not lock volume. Attempting dismount...\n";
        }
        DeviceIoControl(hDevice.Get(), FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

        // Retrieve Volume Size
        GET_LENGTH_INFORMATION lengthInfo;
        if (!DeviceIoControl(hDevice.Get(), IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL)) {
            std::wcout << L"Error: Unable to retrieve volume size.\n";
            return false;
        }

        ULONGLONG totalBytes = lengthInfo.Length.QuadPart;
        const DWORD bufferSize = 1024 * 1024; // 1MB chunks
        std::vector<BYTE> buffer(bufferSize);

        std::wcout << L"Starting Secure Overwrite Wipe (" << passes << L" passes) on " << driveLetter << L"...\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 255);

        for (int p = 1; p <= passes; ++p) {
            BYTE fillPattern = (p % 2 == 1) ? 0x00 : 0xFF;
            if (p == passes && passes >= 3) {
                for (size_t i = 0; i < bufferSize; ++i) buffer[i] = static_cast<BYTE>(dis(gen));
            } else {
                memset(buffer.data(), fillPattern, bufferSize);
            }

            LARGE_INTEGER zeroOffset = { 0 };
            SetFilePointerEx(hDevice.Get(), zeroOffset, NULL, FILE_BEGIN);

            ULONGLONG bytesWrittenTotal = 0;
            DWORD bytesWritten = 0;

            while (bytesWrittenTotal < totalBytes) {
                DWORD toWrite = (DWORD)min((ULONGLONG)bufferSize, totalBytes - bytesWrittenTotal);
                if (!WriteFile(hDevice.Get(), buffer.data(), toWrite, &bytesWritten, NULL) || bytesWritten == 0) {
                    std::wcout << L"\nError writing during pass " << p << L". Error: " << GetLastError() << L"\n";
                    break;
                }
                bytesWrittenTotal += bytesWritten;

                double percent = ((double)bytesWrittenTotal / totalBytes) * 100.0;
                std::wcout << L"\rPass " << p << L"/" << passes << L" Progress: " << std::fixed << std::setprecision(1) << percent << L"%" << std::flush;
            }
            std::wcout << L"\nPass " << p << L" complete.\n";
        }

        // Unlock volume
        DeviceIoControl(hDevice.Get(), FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
        hDevice.Close();

        std::wcout << L"Secure erasure complete. Re-initializing partition/filesystem...\n";
        return FormatVolume(driveLetter, L"NTFS", L"SecuredVolume", true);
    }

    static bool CreatePartition(int diskNumber, ULONGLONG sizeMB) {
        std::wstringstream ss;
        ss << "select disk " << diskNumber << "\n";
        if (sizeMB > 0) {
            ss << "create partition primary size=" << sizeMB << "\n";
        } else {
            ss << "create partition primary\n";
        }

        std::wcout << L"Creating partition on Disk " << diskNumber << L"...\n";
        if (DiskpartExecutor::ExecuteScript(ss.str())) {
            std::wcout << L"Partition created successfully.\n";
            return true;
        } else {
            std::wcout << L"Failed to create partition.\n";
            return false;
        }
    }

    static bool DeletePartition(int diskNumber, int partitionNumber) {
        std::wstringstream ss;
        ss << "select disk " << diskNumber << "\n";
        ss << "select partition " << partitionNumber << "\n";
        ss << "delete partition override\n";

        std::wcout << L"Deleting Partition " << partitionNumber << L" on Disk " << diskNumber << L"...\n";
        if (DiskpartExecutor::ExecuteScript(ss.str())) {
            std::wcout << L"Partition deleted successfully.\n";
            return true;
        } else {
            std::wcout << L"Failed to delete partition.\n";
            return false;
        }
    }
};
