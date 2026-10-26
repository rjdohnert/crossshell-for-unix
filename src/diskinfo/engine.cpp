#include "engine.hpp"
#include <algorithm>
#include <cwchar>

std::wstring StringHelper::CsvBlk(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t c : value) out += (c == L'"' ? L"\"\"" : std::wstring(1, c));
    return out + L'"';
}

std::wstring StringHelper::JsonBlk(const std::wstring& value) {
    static constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring out;
    for (wchar_t c : value) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\b': out += L"\\b"; break;
            case L'\f': out += L"\\f"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c >= 0 && c < 0x20) {
                    unsigned int code = static_cast<unsigned int>(c);
                    out += L"\\u00";
                    out += hex[(code >> 4) & 0xf];
                    out += hex[code & 0xf];
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::wstring StringHelper::ToLowerCopy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

bool StringHelper::ContainsICase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::wstring::npos;
}

std::wstring StringHelper::HumanSize(ULONGLONG bytes) {
    double value = static_cast<double>(bytes);
    const wchar_t* suffixes[] = { L"B", L"K", L"M", L"G", L"T", L"P" };
    size_t suffixIndex = 0;

    while (value >= 1024.0 && suffixIndex + 1 < (sizeof(suffixes) / sizeof(suffixes[0]))) {
        value /= 1024.0;
        ++suffixIndex;
    }

    wchar_t buffer[64] = {};
    if (suffixIndex == 0) {
        swprintf_s(buffer, L"%.0f%s", value, suffixes[suffixIndex]);
    } else if (value < 10.0) {
        swprintf_s(buffer, L"%.1f%s", value, suffixes[suffixIndex]);
    } else {
        swprintf_s(buffer, L"%.0f%s", value, suffixes[suffixIndex]);
    }
    return buffer;
}

std::wstring BlockDeviceInspector::RemoteShareForDrive(const std::wstring& driveRoot) {
    if (driveRoot.size() < 2 || driveRoot[1] != L':') {
        return L"";
    }

    wchar_t localName[3] = { driveRoot[0], L':', L'\0' };
    wchar_t remoteName[MAX_PATH] = {};
    DWORD remoteLen = MAX_PATH;
    DWORD result = WNetGetConnectionW(localName, remoteName, &remoteLen);
    return (result == NO_ERROR) ? remoteName : L"";
}

BlockDeviceRow BlockDeviceInspector::BuildRow(const std::wstring& driveRoot) {
    BlockDeviceRow row;
    row.name = driveRoot.size() >= 2 && driveRoot[1] == L':'
        ? driveRoot.substr(0, 2)
        : driveRoot;
    row.mountPoint = driveRoot;
    row.readOnly = L"unknown";

    UINT driveType = GetDriveTypeW(driveRoot.c_str());
    switch (driveType) {
        case DRIVE_CDROM:
            row.type = L"rom";
            row.readOnly = L"yes";
            break;
        case DRIVE_REMOTE:
            row.type = L"network";
            break;
        case DRIVE_REMOVABLE: row.type = L"removable"; break;
        case DRIVE_FIXED: row.type = L"volume"; break;
        case DRIVE_RAMDISK: row.type = L"ram"; break;
        case DRIVE_UNKNOWN: row.type = L"unknown"; break;
        default:
            row.type = L"unknown";
            break;
    }

    wchar_t fsName[MAX_PATH] = {};
    DWORD volumeFlags = 0;
    if (GetVolumeInformationW(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, &volumeFlags, fsName, MAX_PATH)) {
        row.fsType = fsName;
        row.readOnly = (volumeFlags & FILE_READ_ONLY_VOLUME) != 0 ? L"yes" : L"no";
    } else {
        row.fsType = L"unknown";
    }

    ULARGE_INTEGER freeBytesAvail = {};
    ULARGE_INTEGER totalBytes = {};
    ULARGE_INTEGER totalFreeBytes = {};
    if (GetDiskFreeSpaceExW(driveRoot.c_str(), &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
        row.sizeBytes = totalBytes.QuadPart;
        row.sizeKnown = true;
        row.size = StringHelper::HumanSize(totalBytes.QuadPart);
    } else {
        row.size = L"n/a";
    }

    if (driveType == DRIVE_REMOTE) {
        std::wstring remote = RemoteShareForDrive(driveRoot);
        if (!remote.empty()) {
            row.name = remote;
        }
    }

    return row;
}

static void AppendPhysicalDisks(std::vector<BlockDeviceRow>& rows) {
    for (DWORD diskNumber = 0; diskNumber < 64; ++diskNumber) {
        const std::wstring diskName = L"PhysicalDrive" + std::to_wstring(diskNumber);
        const std::wstring diskPath = L"\\\\.\\" + diskName;
        HANDLE disk = CreateFileW(diskPath.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (disk == INVALID_HANDLE_VALUE) {
            disk = CreateFileW(diskPath.c_str(), FILE_READ_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        }
        if (disk == INVALID_HANDLE_VALUE) continue;

        BlockDeviceRow diskRow;
        diskRow.name = diskName;
        diskRow.type = L"disk";
        diskRow.readOnly = L"unknown";
        diskRow.size = L"n/a";

        DWORD returned = 0;
        GET_LENGTH_INFORMATION lengthInfo{};
        returned = 0;
        if (DeviceIoControl(disk, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0,
                            &lengthInfo, sizeof(lengthInfo), &returned, nullptr) &&
            lengthInfo.Length.QuadPart >= 0) {
            diskRow.sizeBytes = static_cast<ULONGLONG>(lengthInfo.Length.QuadPart);
            diskRow.sizeKnown = true;
            diskRow.size = StringHelper::HumanSize(diskRow.sizeBytes);
        } else {
            diskRow.size = L"n/a";
        }
        rows.push_back(diskRow);

        std::vector<BYTE> layoutBuffer(64 * 1024);
        returned = 0;
        if (DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
                            layoutBuffer.data(), static_cast<DWORD>(layoutBuffer.size()),
                            &returned, nullptr) &&
            returned >= offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) {
            const auto* layout = reinterpret_cast<const DRIVE_LAYOUT_INFORMATION_EX*>(layoutBuffer.data());
            const size_t available = (returned - offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) /
                                     sizeof(PARTITION_INFORMATION_EX);
            const size_t count = (std::min)(static_cast<size_t>(layout->PartitionCount), available);
            for (size_t index = 0; index < count; ++index) {
                const PARTITION_INFORMATION_EX& partition = layout->PartitionEntry[index];
                if (partition.PartitionNumber == 0 || partition.PartitionLength.QuadPart <= 0 ||
                    partition.PartitionStyle == PARTITION_STYLE_RAW) continue;
                BlockDeviceRow partRow;
                partRow.name = diskName + L"p" + std::to_wstring(partition.PartitionNumber);
                partRow.type = L"part";
                partRow.parent = diskName;
                partRow.mountPoint = L"";
                partRow.fsType = L"unknown";
                partRow.sizeBytes = static_cast<ULONGLONG>(partition.PartitionLength.QuadPart);
                partRow.sizeKnown = true;
                partRow.size = StringHelper::HumanSize(partRow.sizeBytes);
                partRow.readOnly = L"unknown";
                rows.push_back(std::move(partRow));
            }
        }
        CloseHandle(disk);
    }
}

static std::vector<std::wstring> GetVolumeMountPoints(const std::wstring& volumeName) {
    DWORD required = 0;
    GetVolumePathNamesForVolumeNameW(volumeName.c_str(), nullptr, 0, &required);
    if (GetLastError() != ERROR_MORE_DATA || required == 0) return {};
    std::vector<wchar_t> buffer(required, L'\0');
    if (!GetVolumePathNamesForVolumeNameW(volumeName.c_str(), buffer.data(), required, &required)) {
        return {};
    }
    std::vector<std::wstring> paths;
    for (const wchar_t* path = buffer.data(); *path; path += wcslen(path) + 1) {
        paths.emplace_back(path);
    }
    return paths;
}

static std::wstring GetVolumeParentDisks(const std::wstring& volumeName) {
    std::wstring devicePath = volumeName;
    if (devicePath.rfind(L"\\\\?\\Volume{", 0) == 0) {
        while (!devicePath.empty() && devicePath.back() == L'\\') devicePath.pop_back();
    } else if (devicePath.size() >= 2 && devicePath[1] == L':') {
        devicePath = L"\\\\.\\" + devicePath.substr(0, 2);
    } else {
        return L"";
    }
    HANDLE volume = CreateFileW(devicePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (volume == INVALID_HANDLE_VALUE) return L"";

    std::vector<BYTE> buffer(sizeof(VOLUME_DISK_EXTENTS) + 16 * sizeof(DISK_EXTENT));
    DWORD returned = 0;
    std::wstring parents;
    if (DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0,
                        buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) &&
        returned >= offsetof(VOLUME_DISK_EXTENTS, Extents)) {
        const auto* extents = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buffer.data());
        const size_t available = (returned - offsetof(VOLUME_DISK_EXTENTS, Extents)) / sizeof(DISK_EXTENT);
        const size_t count = (std::min)(static_cast<size_t>(extents->NumberOfDiskExtents), available);
        for (size_t index = 0; index < count; ++index) {
            if (!parents.empty()) parents += L";";
            parents += L"PhysicalDrive" + std::to_wstring(extents->Extents[index].DiskNumber);
        }
    }
    CloseHandle(volume);
    return parents;
}

static bool GetVolumePartitionInfo(const std::wstring& volumeName,
                                   PARTITION_INFORMATION_EX& partition) {
    std::wstring devicePath = volumeName;
    if (devicePath.rfind(L"\\\\?\\Volume{", 0) == 0) {
        while (!devicePath.empty() && devicePath.back() == L'\\') devicePath.pop_back();
    } else if (devicePath.size() >= 2 && devicePath[1] == L':') {
        devicePath = L"\\\\.\\" + devicePath.substr(0, 2);
    } else {
        return false;
    }
    HANDLE volume = CreateFileW(devicePath.c_str(), FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (volume == INVALID_HANDLE_VALUE) return false;
    DWORD returned = 0;
    const BOOL result = DeviceIoControl(volume, IOCTL_DISK_GET_PARTITION_INFO_EX,
                                        nullptr, 0, &partition, sizeof(partition),
                                        &returned, nullptr);
    CloseHandle(volume);
    return result && returned >= sizeof(partition) && partition.PartitionNumber != 0 &&
           partition.PartitionLength.QuadPart > 0;
}

std::vector<BlockDeviceRow> BlockDeviceInspector::EnumerateRows() {
    std::vector<BlockDeviceRow> rows;
    AppendPhysicalDisks(rows);
    std::vector<std::wstring> knownPartitions;

    std::vector<std::wstring> volumeNames;
    wchar_t volumeName[MAX_PATH] = {};
    HANDLE findVolume = FindFirstVolumeW(volumeName, MAX_PATH);
    if (findVolume != INVALID_HANDLE_VALUE) {
        do {
            volumeNames.emplace_back(volumeName);
        } while (FindNextVolumeW(findVolume, volumeName, MAX_PATH));
        FindVolumeClose(findVolume);
    }

    for (const std::wstring& volume : volumeNames) {
        const std::vector<std::wstring> mounts = GetVolumeMountPoints(volume);
        const std::wstring queryPath = mounts.empty() ? volume : mounts.front();
        BlockDeviceRow row = BuildRow(queryPath);
        row.name = mounts.empty() ? volume : mounts.front();
        row.mountPoint.clear();
        for (const std::wstring& mount : mounts) {
            if (!row.mountPoint.empty()) row.mountPoint += L";";
            row.mountPoint += mount;
        }
        row.parent = GetVolumeParentDisks(volume);

        PARTITION_INFORMATION_EX partition{};
        if (GetVolumePartitionInfo(volume, partition)) {
            const std::wstring key = row.parent + L"p" + std::to_wstring(partition.PartitionNumber);
            auto existing = std::find_if(rows.begin(), rows.end(), [&key](const BlockDeviceRow& candidate) {
                return candidate.type == L"part" && candidate.name == key;
            });
            if (existing != rows.end()) {
                existing->mountPoint = row.mountPoint;
                existing->fsType = row.fsType;
                existing->readOnly = row.readOnly;
            } else if (std::find(knownPartitions.begin(), knownPartitions.end(), key) == knownPartitions.end()) {
                knownPartitions.push_back(key);
                BlockDeviceRow partitionRow;
                partitionRow.name = key;
                partitionRow.type = L"part";
                partitionRow.parent = row.parent;
                partitionRow.mountPoint = row.mountPoint;
                partitionRow.fsType = row.fsType;
                partitionRow.sizeBytes = static_cast<ULONGLONG>(partition.PartitionLength.QuadPart);
                partitionRow.sizeKnown = true;
                partitionRow.size = StringHelper::HumanSize(partitionRow.sizeBytes);
                partitionRow.readOnly = row.readOnly;
                rows.push_back(std::move(partitionRow));
            }
        }
        rows.push_back(std::move(row));
    }

    DWORD length = GetLogicalDriveStringsW(0, nullptr);
    if (length > 0) {
        std::vector<wchar_t> driveBuffer(length + 1, L'\0');
        if (GetLogicalDriveStringsW(length, driveBuffer.data()) != 0) {
            for (wchar_t* drive = driveBuffer.data(); *drive; drive += wcslen(drive) + 1) {
                if (GetDriveTypeW(drive) == DRIVE_REMOTE) rows.push_back(BuildRow(drive));
            }
        }
    }

    return rows;
}
