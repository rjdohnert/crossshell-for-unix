#include "engine.hpp"

Win32Handle::Win32Handle(HANDLE h) : h_(h) {}

Win32Handle::~Win32Handle() {
    if (isValid()) {
        CloseHandle(h_);
    }
}

bool Win32Handle::isValid() const {
    return h_ != INVALID_HANDLE_VALUE && h_ != nullptr;
}

Win32Handle::operator HANDLE() const {
    return h_;
}

std::string StorageManager::WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string output(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &output[0], required, nullptr, nullptr);
    return output;
}

std::vector<VolumeGroup> StorageManager::DiscoverVolumeGroups() {
    std::vector<VolumeGroup> vgs;

    // Group 1: 'rootvg' (System Disk & Operating System Partitions)
    VolumeGroup rootvg;
    rootvg.name = "rootvg";
    rootvg.vgIdentifier = "00f6123a00004c00";

    // Query PhysicalDrive0 (Primary System Drive)
    PhysicalVolume pv0;
    pv0.name = "hdisk0";
    pv0.deviceNumber = 0;

    Win32Handle hDrive = CreateFileW(
        L"\\\\.\\PhysicalDrive0",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDrive.isValid()) {
        GET_LENGTH_INFORMATION lengthInfo{};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, nullptr)) {
            pv0.totalSizeBytes = lengthInfo.Length.QuadPart;
        } else {
            pv0.totalSizeBytes = 512ULL * 1024 * 1024 * 1024;
        }

        pv0.totalPPs = static_cast<UINT32>(pv0.totalSizeBytes / PP_SIZE_BYTES);
        pv0.freePPs = static_cast<UINT32>(pv0.totalPPs * 0.25);
        rootvg.pvs.push_back(pv0);

        wchar_t driveBuffer[4096] = {};
        DWORD driveChars = GetLogicalDriveStringsW(ARRAYSIZE(driveBuffer), driveBuffer);
        wchar_t* pDrive = driveBuffer;
        while (driveChars > 0 && *pDrive) {
            LogicalVolume lv;
            lv.name = "lv_" + WideToUtf8(std::wstring(pDrive));
            lv.type = "ntfs";
            lv.lpCount = 1;
            lv.ppCount = 1;
            lv.pvCount = 1;
            lv.state = "open/syncd";
            lv.mountPoint = WideToUtf8(std::wstring(pDrive));
            rootvg.lvs.push_back(lv);
            pDrive += wcslen(pDrive) + 1;
            driveChars = static_cast<DWORD>(driveBuffer + ARRAYSIZE(driveBuffer) - pDrive);
        }
    }

    rootvg.totalSizeBytes = pv0.totalSizeBytes;
    rootvg.freeSizeBytes = static_cast<UINT64>(pv0.freePPs) * PP_SIZE_BYTES;
    vgs.push_back(rootvg);

    Win32Handle hDrive1 = CreateFileW(
        L"\\\\.\\PhysicalDrive1",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDrive1.isValid()) {
        VolumeGroup datavg;
        datavg.name = "datavg";
        datavg.vgIdentifier = "00f6123b77a11e00";

        PhysicalVolume pv1;
        pv1.name = "hdisk1";
        pv1.deviceNumber = 1;

        GET_LENGTH_INFORMATION lengthInfo{};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDrive1, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, nullptr)) {
            pv1.totalSizeBytes = lengthInfo.Length.QuadPart;
        } else {
            pv1.totalSizeBytes = 1024ULL * 1024 * 1024 * 1024;
        }

        pv1.totalPPs = static_cast<UINT32>(pv1.totalSizeBytes / PP_SIZE_BYTES);
        pv1.freePPs = static_cast<UINT32>(pv1.totalPPs * 0.40);
        datavg.pvs.push_back(pv1);

        LogicalVolume lvData;
        lvData.name = "lv_data01";
        lvData.type = "ntfs";
        lvData.ppCount = pv1.totalPPs - pv1.freePPs;
        lvData.lpCount = lvData.ppCount;
        lvData.pvCount = 1;
        lvData.state = "open/syncd";
        lvData.mountPoint = "E:\\";
        datavg.lvs.push_back(lvData);

        datavg.totalSizeBytes = pv1.totalSizeBytes;
        datavg.freeSizeBytes = static_cast<UINT64>(pv1.freePPs) * PP_SIZE_BYTES;

        vgs.push_back(datavg);
    }

    return vgs;
}
