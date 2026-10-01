/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstdlib>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")

// ============================================================================
// 1. RAII GUARDS & DYNAMIC APIS
// ============================================================================

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFileHandle() { Close(); }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = INVALID_HANDLE_VALUE; }
    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedFindVolumeHandle {
public:
    explicit ScopedFindVolumeHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFindVolumeHandle() { Close(); }

    ScopedFindVolumeHandle(const ScopedFindVolumeHandle&) = delete;
    ScopedFindVolumeHandle& operator=(const ScopedFindVolumeHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FindVolumeClose(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedLibraryHandle {
public:
    explicit ScopedLibraryHandle(HMODULE handle = nullptr) : m_handle(handle) {}
    ~ScopedLibraryHandle() { Close(); }

    ScopedLibraryHandle(const ScopedLibraryHandle&) = delete;
    ScopedLibraryHandle& operator=(const ScopedLibraryHandle&) = delete;

    HMODULE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FreeLibrary(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HMODULE m_handle;
};

class ScopedSid {
public:
    explicit ScopedSid(PSID sid = nullptr) : m_sid(sid) {}
    ~ScopedSid() { Close(); }

    ScopedSid(const ScopedSid&) = delete;
    ScopedSid& operator=(const ScopedSid&) = delete;

    PSID Get() const { return m_sid; }
    bool IsValid() const { return m_sid != nullptr; }

    void Close() {
        if (IsValid()) {
            FreeSid(m_sid);
            m_sid = nullptr;
        }
    }

private:
    PSID m_sid;
};

typedef VOID(WINAPI* PFMIFSCALLBACK)(ULONG Command, ULONG Action, PVOID Data);
typedef BOOLEAN(WINAPI* PFORMATEX)(
    PWCHAR DriveRoot,
    ULONG MediaFormat,
    PWCHAR Format,
    PWCHAR Label,
    BOOLEAN Quick,
    ULONG ClusterSize,
    PFMIFSCALLBACK Callback
);

// Callback structure for native FormatEx progress output
static VOID WINAPI FormatCallback(ULONG Command, ULONG Action, PVOID Data) {
    if (Command == 0x09) { // FMIFS_PROGRESS
        ULONG percent = *(ULONG*)Data;
        std::wcout << L"\rFormatting... " << percent << L"%" << std::flush;
    }
}

// ============================================================================
// 2. SECURITY & ERROR HELPERS
// ============================================================================

class SecurityInspector {
public:
    static bool IsUserAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            ScopedSid scopedSid(adminGroup);
            CheckTokenMembership(NULL, scopedSid.Get(), &isAdmin);
        }
        return isAdmin != FALSE;
    }

    static std::wstring GetErrorMessage(DWORD errorCode) {
        LPWSTR messageBuffer = nullptr;
        size_t size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&messageBuffer, 0, NULL);

        std::wstring message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
    }
};

// ============================================================================
// 3. DISKPART SCRIPT EXECUTOR
// ============================================================================

class DiskpartExecutor {
public:
    static bool ExecuteScript(const std::wstring& scriptContent) {
        WCHAR tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring scriptPath = std::wstring(tempPath) + L"fdisk_script.txt";

        ScopedFileHandle hFile(CreateFileW(scriptPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hFile.IsValid()) return false;

        int scriptBytes = WideCharToMultiByte(CP_ACP, 0, scriptContent.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (scriptBytes <= 0) {
            hFile.Close();
            DeleteFileW(scriptPath.c_str());
            return false;
        }

        std::string ansiScript(static_cast<size_t>(scriptBytes - 1), '\0');
        if (WideCharToMultiByte(CP_ACP, 0, scriptContent.c_str(), -1, ansiScript.data(), scriptBytes, nullptr, nullptr) <= 0) {
            hFile.Close();
            DeleteFileW(scriptPath.c_str());
            return false;
        }

        DWORD bytesWritten;
        WriteFile(hFile.Get(), ansiScript.c_str(), (DWORD)ansiScript.size(), &bytesWritten, NULL);
        hFile.Close();

        std::wstring command = L"diskpart.exe /s \"" + scriptPath + L"\"";
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        bool success = false;
        if (CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            success = (exitCode == 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        DeleteFileW(scriptPath.c_str());
        return success;
    }
};

// ============================================================================
// 4. DISK & VOLUME MANAGEMENT ENGINE
// ============================================================================

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

// ============================================================================
// 5. APPLICATION CONTROLLER & CLI PARSER
// ============================================================================

class FdiskApplication {
public:
    static void ShowVersion() {
        std::wcout << L"fdisk version 1.0.0\n";
        std::wcout << L"Copyright (C) Roberto J Dohnert. All rights reserved.\n";
    }

    static void ShowHelp() {
        std::wcout << LR"(fdisk(1)            CrossShell for UNIX Reference Manual                 fdisk(1)

    NAME
        fdisk - manage Windows disks, partitions, and volumes

    SYNOPSIS
        fdisk [OPTIONS] [COMMAND] [ARGUMENTS...]

    DESCRIPTION
        fdisk provides disk partition table and volume management facilities for
        Windows NT. It allows listing disks and volumes, mounting and unmounting
        volume GUIDs to drive letters, relabeling volumes, creating and deleting
        partitions, and formatting or securely wiping file systems. Administrator
        privileges are required for partition and format operations.

    OPTIONS
        -l, --list
            List all physical disks, partitions, and mounted volumes.

        -m, --mount LETTER VOLUME_GUID
            Mount a volume GUID to the specified drive letter.

        -u, --unmount LETTER
            Unmount and remove the specified drive letter.

        -L, --label LETTER NAME
            Set the volume label for the specified drive letter.

        -f, --format LETTER
            Format the specified drive volume (NTFS, FAT32, or exFAT).

        -S, --secure-format LETTER [PASSES]
            Securely erase volume data with multi-pass wipe before formatting.

        -c, --create-partition DISK SIZE_MB
            Create a new primary partition of specified size in MB on a disk.

        -d, --delete-partition DISK PART_NUM
            Delete a specific partition number on a disk.

        --fs FILESYSTEM
            Specify filesystem type for format (ntfs, fat32, exfat; default: ntfs).

        --quick
            Perform a quick format operation.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        fdisk --list
            List all detected storage devices, partitions, and volumes.

        fdisk --mount E: \\?\Volume{12345678-1234-1234-1234-1234567890ab}\
            Mount a volume GUID to drive letter E:.

        fdisk --unmount E:
            Unmount drive letter E:.

        fdisk --format E: --fs ntfs --quick
            Quick-format drive E: with the NTFS filesystem.

        fdisk --create-partition 1 10240
            Create a 10 GB partition on physical disk 1.

    CrossShell for UNIX                                                    fdisk(1)
)";
    }

    int Run(int argc, wchar_t* argv[]) const {
        if (!SecurityInspector::IsUserAdmin()) {
            std::wcout << L"ERROR: Administrator privileges are required to run fdisk.\n";
            std::wcout << L"Please re-run this command prompt as Administrator.\n";
            return 1;
        }

        if (argc < 2) {
            ShowHelp();
            return 0;
        }

        std::wstring command;
        int arg_idx = 1;

        if (argv[arg_idx] == L"--") {
            if (arg_idx + 1 >= argc) {
                ShowHelp();
                return 0;
            }
            ++arg_idx;
        }

        command = argv[arg_idx];

        if (command == L"--help" || command == L"-h" || command == L"/?") {
            ShowHelp();
            return 0;
        }

        if (command == L"--version" || command == L"-v") {
            ShowVersion();
            return 0;
        }

        if (command == L"--list" || command == L"-l") {
            VolumeManager::ListDisksAndVolumes();
            return 0;
        }

        if ((command == L"--mount" || command == L"-m") && argc - arg_idx >= 3) {
            return VolumeManager::MountVolume(argv[arg_idx + 1], argv[arg_idx + 2]) ? 0 : 1;
        }

        if ((command == L"--unmount" || command == L"-u") && argc - arg_idx >= 2) {
            return VolumeManager::UnmountVolume(argv[arg_idx + 1]) ? 0 : 1;
        }

        if ((command == L"--label" || command == L"-L") && argc - arg_idx >= 3) {
            return VolumeManager::SetLabel(argv[arg_idx + 1], argv[arg_idx + 2]) ? 0 : 1;
        }

        if ((command == L"--format" || command == L"-f") && argc - arg_idx >= 2) {
            std::wstring drive = argv[arg_idx + 1];
            std::wstring fs = L"NTFS";
            std::wstring label = L"New Volume";
            bool quick = false;

            for (int i = arg_idx + 2; i < argc; ++i) {
                std::wstring arg = argv[i];
                if ((arg == L"--fs" || arg == L"-F") && i + 1 < argc) fs = argv[++i];
                else if ((arg == L"--label" || arg == L"-L") && i + 1 < argc) label = argv[++i];
                else if (arg == L"--quick" || arg == L"-q") quick = true;
                else {
                    std::wcout << L"Unknown option: " << arg << L"\n";
                    ShowHelp();
                    return 1;
                }
            }

            return VolumeManager::FormatVolume(drive, fs, label, quick) ? 0 : 1;
        }

        if ((command == L"--secure-format" || command == L"-S") && argc - arg_idx >= 2) {
            std::wstring drive = argv[arg_idx + 1];
            int passes = (argc - arg_idx >= 3) ? _wtoi(argv[arg_idx + 2]) : 1;
            if (passes <= 0) passes = 1;
            return VolumeManager::SecureFormatVolume(drive, passes) ? 0 : 1;
        }

        if ((command == L"--create-partition" || command == L"-c") && argc - arg_idx >= 2) {
            int disk = _wtoi(argv[arg_idx + 1]);
            ULONGLONG sizeMB = (argc - arg_idx >= 3) ? wcstoull(argv[arg_idx + 2], NULL, 10) : 0;
            return VolumeManager::CreatePartition(disk, sizeMB) ? 0 : 1;
        }

        if ((command == L"--delete-partition" || command == L"-d") && argc - arg_idx >= 3) {
            int disk = _wtoi(argv[arg_idx + 1]);
            int part = _wtoi(argv[arg_idx + 2]);
            return VolumeManager::DeletePartition(disk, part) ? 0 : 1;
        }

        std::wcout << L"Invalid parameters or missing arguments. Use --help for guidance.\n";
        return 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    FdiskApplication app;
    return app.Run(argc, argv);
}
