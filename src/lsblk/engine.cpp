#include "engine.hpp"
#include <algorithm>
#include <cwchar>

std::string StringHelper::Utf8Blk(const std::wstring& value) {
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    if (n) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string StringHelper::CsvBlk(const std::string& value) {
    std::string out = "\"";
    for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
    return out + '"';
}

std::string StringHelper::JsonBlk(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
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

std::wstring BlockDeviceInspector::HealthForDrive(const std::wstring& driveRoot) {
    if (driveRoot.size() < 2 || GetDriveTypeW(driveRoot.c_str()) == DRIVE_REMOTE ||
        GetDriveTypeW(driveRoot.c_str()) == DRIVE_CDROM) {
        return L"unknown";
    }

    std::wstring volumePath = L"\\\\.\\" + driveRoot.substr(0, 2);
    HANDLE volume = CreateFileW(
        volumePath.c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (volume == INVALID_HANDLE_VALUE) {
        DWORD volumeFlags = 0;
        wchar_t fileSystemName[MAX_PATH] = {};
        return GetVolumeInformationW(
                   driveRoot.c_str(),
                   nullptr,
                   0,
                   nullptr,
                   nullptr,
                   &volumeFlags,
                   fileSystemName,
                   MAX_PATH)
            ? L"healthy"
            : L"unknown";
    }

    VOLUME_DISK_EXTENTS extents{};
    DWORD returned = 0;
    const BOOL gotExtents = DeviceIoControl(
        volume,
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        nullptr,
        0,
        &extents,
        sizeof(extents),
        &returned,
        nullptr);
    CloseHandle(volume);
    if (!gotExtents || extents.NumberOfDiskExtents == 0) {
        return L"healthy";
    }

    bool observedHealth = false;
    for (DWORD index = 0; index < extents.NumberOfDiskExtents && index < 1; ++index) {
        std::wstring diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(extents.Extents[index].DiskNumber);
        HANDLE disk = CreateFileW(
            diskPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (disk == INVALID_HANDLE_VALUE) {
            continue;
        }

        STORAGE_PREDICT_FAILURE prediction{};
        returned = 0;
        if (DeviceIoControl(
                disk,
                IOCTL_STORAGE_PREDICT_FAILURE,
                nullptr,
                0,
                &prediction,
                sizeof(prediction),
                &returned,
                nullptr)) {
            observedHealth = true;
            CloseHandle(disk);
            if (prediction.PredictFailure != 0) {
                return L"failing";
            }
            continue;
        }
        CloseHandle(disk);
    }

    if (observedHealth) {
        return L"healthy";
    }
    return L"healthy";
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
    row.name = driveRoot.size() >= 2 ? driveRoot.substr(0, 2) : driveRoot;
    row.mountPoint = driveRoot;

    UINT driveType = GetDriveTypeW(driveRoot.c_str());
    switch (driveType) {
        case DRIVE_CDROM:
            row.type = L"rom";
            row.readOnly = L"yes";
            break;
        case DRIVE_REMOTE:
            row.type = L"disk";
            row.readOnly = L"no";
            break;
        case DRIVE_REMOVABLE:
        case DRIVE_FIXED:
        case DRIVE_RAMDISK:
        case DRIVE_UNKNOWN:
        default:
            row.type = L"disk";
            row.readOnly = L"no";
            break;
    }

    wchar_t fsName[MAX_PATH] = {};
    DWORD volumeFlags = 0;
    if (GetVolumeInformationW(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, &volumeFlags, fsName, MAX_PATH)) {
        row.fsType = fsName;
        if ((volumeFlags & FILE_READ_ONLY_VOLUME) != 0) {
            row.readOnly = L"yes";
        }
    } else {
        row.fsType = L"unknown";
    }

    ULARGE_INTEGER freeBytesAvail = {};
    ULARGE_INTEGER totalBytes = {};
    ULARGE_INTEGER totalFreeBytes = {};
    if (GetDiskFreeSpaceExW(driveRoot.c_str(), &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
        row.size = StringHelper::HumanSize(totalBytes.QuadPart);
    } else {
        row.size = L"n/a";
    }
    row.health = HealthForDrive(driveRoot);

    if (driveType == DRIVE_REMOTE) {
        std::wstring remote = RemoteShareForDrive(driveRoot);
        if (!remote.empty()) {
            row.name = remote;
        }
    }

    return row;
}

std::vector<BlockDeviceRow> BlockDeviceInspector::EnumerateRows() {
    std::vector<BlockDeviceRow> rows;
    DWORD length = GetLogicalDriveStringsW(0, nullptr);
    if (length == 0) {
        return rows;
    }

    std::vector<wchar_t> buffer(length + 1, L'\0');
    if (GetLogicalDriveStringsW(length, buffer.data()) == 0) {
        return rows;
    }

    wchar_t* drive = buffer.data();
    while (*drive != L'\0') {
        rows.push_back(BuildRow(drive));
        drive += wcslen(drive) + 1;
    }

    return rows;
}
