#include "storage_provider.hpp"
#include <algorithm>

using namespace sysctl;

// ============================================================================
// DeviceHandle implementation
// ============================================================================

DeviceHandle::DeviceHandle(const std::wstring& path)
    : m_handle(CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr)) {
    if (m_handle == INVALID_HANDLE_VALUE) {
        ThrowWindowsError(GetLastError(), "CreateFileW(" + WideToUtf8(path) + ")");
    }
}

DeviceHandle::~DeviceHandle() {
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
    }
}

HANDLE DeviceHandle::Get() const noexcept {
    return m_handle;
}

bool DeviceHandle::IsValid() const noexcept {
    return m_handle != INVALID_HANDLE_VALUE;
}

// ============================================================================
// WindowsStorageFactory implementation
// ============================================================================

namespace {

class DiskInterfaces {
public:
    HDEVINFO value;
    DiskInterfaces()
        : value(SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr,
                                     DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)) {
        if (value == INVALID_HANDLE_VALUE) ThrowWindowsError(GetLastError(), "SetupDiGetClassDevsW");
    }
    ~DiskInterfaces() { SetupDiDestroyDeviceInfoList(value); }
    DiskInterfaces(const DiskInterfaces&) = delete;
    DiskInterfaces& operator=(const DiskInterfaces&) = delete;
};

DWORD Control(HANDLE device, DWORD code, void* input, DWORD inputSize,
              void* output, DWORD outputSize, const char* operation) {
    DWORD returned = 0;
    if (!DeviceIoControl(device, code, input, inputSize, output, outputSize, &returned, nullptr)) {
        ThrowWindowsError(GetLastError(), operation);
    }
    return returned;
}

std::wstring DiskPath(DWORD number) {
    return L"\\\\.\\PhysicalDrive" + std::to_wstring(number);
}

std::vector<DWORD> DiscoverDisks() {
    DiskInterfaces interfaces;
    std::vector<DWORD> disks;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInterfaces(interfaces.value, nullptr, &GUID_DEVINTERFACE_DISK,
                                         index, &data)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            ThrowWindowsError(error, "SetupDiEnumDeviceInterfaces");
        }
        DWORD size = 0;
        if (SetupDiGetDeviceInterfaceDetailW(interfaces.value, &data, nullptr, 0, &size, nullptr) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            throw std::runtime_error("Cannot determine disk interface path size");
        }
        if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 1024 * 1024) {
            throw std::runtime_error("Invalid disk interface path size");
        }
        std::vector<BYTE> buffer(size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(interfaces.value, &data, detail, size, nullptr, nullptr)) {
            ThrowWindowsError(GetLastError(), "SetupDiGetDeviceInterfaceDetailW");
        }
        DeviceHandle disk(detail->DevicePath);
        STORAGE_DEVICE_NUMBER number{};
        if (Control(disk.Get(), IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number,
                    sizeof(number), "IOCTL_STORAGE_GET_DEVICE_NUMBER") < sizeof(number)) {
            throw std::runtime_error("Truncated storage device number");
        }
        disks.push_back(number.DeviceNumber);
    }
    std::sort(disks.begin(), disks.end());
    disks.erase(std::unique(disks.begin(), disks.end()), disks.end());
    return disks;
}

DISK_GEOMETRY_EX ReadGeometry(DWORD number) {
    DeviceHandle disk(DiskPath(number));
    BYTE buffer[4096]{};
    const DWORD returned = Control(disk.Get(), IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0,
                                    buffer, sizeof(buffer), "IOCTL_DISK_GET_DRIVE_GEOMETRY_EX");
    if (returned < offsetof(DISK_GEOMETRY_EX, Data)) {
        throw std::runtime_error("Truncated disk geometry");
    }
    DISK_GEOMETRY_EX geometry{};
    std::memcpy(&geometry, buffer, (std::min)(sizeof(geometry), static_cast<size_t>(returned)));
    if (geometry.DiskSize.QuadPart < 0 || geometry.Geometry.BytesPerSector == 0) {
        throw std::runtime_error("Invalid disk geometry");
    }
    return geometry;
}

std::vector<BYTE> ReadDescriptor(DWORD number) {
    DeviceHandle disk(DiskPath(number));
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    STORAGE_DESCRIPTOR_HEADER header{};
    if (Control(disk.Get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header,
                sizeof(header), "IOCTL_STORAGE_QUERY_PROPERTY(header)") < sizeof(header) ||
        header.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR) || header.Size > 1024 * 1024) {
        throw std::runtime_error("Invalid storage descriptor size");
    }
    std::vector<BYTE> buffer(header.Size);
    const DWORD returned = Control(disk.Get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                                    buffer.data(), header.Size, "IOCTL_STORAGE_QUERY_PROPERTY");
    if (returned < sizeof(STORAGE_DEVICE_DESCRIPTOR) || returned > buffer.size()) {
        throw std::runtime_error("Truncated storage descriptor");
    }
    buffer.resize(returned);
    return buffer;
}

std::string DescriptorValue(DWORD number, const std::string& field) {
    const auto bytes = ReadDescriptor(number);
    STORAGE_DEVICE_DESCRIPTOR descriptor{};
    std::memcpy(&descriptor, bytes.data(), sizeof(descriptor));
    if (field == "model") return WindowsStorageFactory::DescriptorString(bytes, descriptor.ProductIdOffset);
    if (field == "vendor") return WindowsStorageFactory::DescriptorString(bytes, descriptor.VendorIdOffset);
    if (field == "serial") return WindowsStorageFactory::DescriptorString(bytes, descriptor.SerialNumberOffset);
    if (field == "revision") return WindowsStorageFactory::DescriptorString(bytes, descriptor.ProductRevisionOffset);
    if (field == "removable") return descriptor.RemovableMedia ? "1" : "0";
    switch (descriptor.BusType) {
        case BusTypeScsi: return "SCSI";
        case BusTypeAtapi: return "ATAPI";
        case BusTypeAta: return "ATA";
        case BusType1394: return "1394";
        case BusTypeSsa: return "SSA";
        case BusTypeFibre: return "Fibre";
        case BusTypeUsb: return "USB";
        case BusTypeRAID: return "RAID";
        case BusTypeiScsi: return "iSCSI";
        case BusTypeSas: return "SAS";
        case BusTypeSata: return "SATA";
        case BusTypeSd: return "SD";
        case BusTypeMmc: return "MMC";
        case BusTypeVirtual: return "Virtual";
        case BusTypeFileBackedVirtual: return "FileBackedVirtual";
        case BusTypeSpaces: return "StorageSpaces";
        case BusTypeNvme: return "NVMe";
        default: return "Unknown(" + std::to_string(descriptor.BusType) + ")";
    }
}

std::vector<char> DiscoverVolumes() {
    const DWORD mask = GetLogicalDrives();
    if (mask == 0) ThrowWindowsError(GetLastError(), "GetLogicalDrives");
    std::vector<char> volumes;
    for (unsigned index = 0; index < 26; ++index) {
        if ((mask & (1UL << index)) == 0) continue;
        const wchar_t root[] = {static_cast<wchar_t>(L'A' + index), L':', L'\\', L'\0'};
        const UINT type = GetDriveTypeW(root);
        if (type != DRIVE_REMOTE && type != DRIVE_NO_ROOT_DIR) {
            volumes.push_back(static_cast<char>('A' + index));
        }
    }
    return volumes;
}

} // namespace

std::string WindowsStorageFactory::DescriptorString(const std::vector<BYTE>& bytes, DWORD offset) {
    if (offset == 0) return {};
    if (offset >= bytes.size()) throw std::runtime_error("Invalid storage descriptor string offset");
    const auto first = bytes.begin() + offset;
    const auto last = std::find(first, bytes.end(), BYTE{0});
    if (last == bytes.end()) throw std::runtime_error("Unterminated storage descriptor string");
    return Trim(std::string(first, last));
}

std::vector<std::string> WindowsStorageFactory::EnumerateDisks() {
    const auto disks = DiscoverDisks();
    std::vector<std::string> result;
    for (DWORD number : disks) {
        result.push_back("PhysicalDrive" + std::to_string(number));
    }
    return result;
}

std::vector<char> WindowsStorageFactory::EnumerateVolumes() {
    return DiscoverVolumes();
}

std::wstring WindowsStorageFactory::VolumePath(char letter) {
    return {static_cast<wchar_t>(letter), L':', L'\\'};
}

std::string WindowsStorageFactory::VolumeSpace(char letter, const std::string& field) {
    ULARGE_INTEGER available{}, total{}, free{};
    if (!GetDiskFreeSpaceExW(VolumePath(letter).c_str(), &available, &total, &free)) {
        ThrowWindowsError(GetLastError(), "GetDiskFreeSpaceExW");
    }
    if (field == "size") return std::to_string(total.QuadPart);
    if (field == "free") return std::to_string(free.QuadPart);
    return std::to_string(available.QuadPart);
}

std::string WindowsStorageFactory::VolumeInfo(char letter, bool label) {
    wchar_t volumeLabel[MAX_PATH + 1]{}, filesystem[MAX_PATH + 1]{};
    if (!GetVolumeInformationW(VolumePath(letter).c_str(), volumeLabel, ARRAYSIZE(volumeLabel),
                               nullptr, nullptr, nullptr, filesystem, ARRAYSIZE(filesystem))) {
        ThrowWindowsError(GetLastError(), "GetVolumeInformationW");
    }
    return WideToUtf8(label ? volumeLabel : filesystem);
}

void WindowsStorageFactory::Populate(MibTree& tree) {
    const auto disks = DiscoverDisks();
    const auto volumes = DiscoverVolumes();
    std::string names;
    for (DWORD number : disks) {
        const std::string name = "PhysicalDrive" + std::to_string(number);
        if (!names.empty()) names += ",";
        names += name;
        const std::string prefix = "hw.disk." + name + ".";
        tree.RegisterNode(prefix + "path", "Windows physical disk device path", [number]() {
            return WideToUtf8(DiskPath(number));
        });
        tree.RegisterNode(prefix + "size", "Physical disk capacity in bytes", [number]() {
            return std::to_string(ReadGeometry(number).DiskSize.QuadPart);
        });
        tree.RegisterNode(prefix + "sectorsize", "Logical sector size in bytes", [number]() {
            return std::to_string(ReadGeometry(number).Geometry.BytesPerSector);
        });
        for (const std::string field : {"model", "vendor", "serial", "revision", "bus", "removable"}) {
            tree.RegisterNode(prefix + field, "Windows storage descriptor " + field, [number, field]() {
                return DescriptorValue(number, field);
            });
        }
    }
    tree.RegisterNode("hw.diskcount", "Number of currently attached Windows disk devices", [disks]() {
        return std::to_string(disks.size());
    });
    tree.RegisterNode("hw.disknames", "Comma-separated Windows physical disk names", [names]() {
        return names;
    });
    tree.RegisterNode("hw.diskstats", "Disk I/O statistics table", []() {
        return "use typeperf to view hw.diskstats information (Windows PhysicalDisk counters)";
    }, nullptr, SysctlLeafNode::Kind::Table);

    std::string volumeNames;
    for (char letter : volumes) {
        if (!volumeNames.empty()) volumeNames += ",";
        volumeNames += letter;
        const std::string prefix = std::string("fs.volume.") + letter + ".";
        tree.RegisterNode(prefix + "path", "Local drive-letter root", [letter]() {
            return WideToUtf8(VolumePath(letter));
        });
        tree.RegisterNode(prefix + "type", "Windows drive type", [letter]() -> std::string {
            switch (GetDriveTypeW(VolumePath(letter).c_str())) {
                case DRIVE_FIXED: return "fixed";
                case DRIVE_REMOVABLE: return "removable";
                case DRIVE_CDROM: return "cdrom";
                case DRIVE_RAMDISK: return "ramdisk";
                case DRIVE_NO_ROOT_DIR: throw std::runtime_error("volume is no longer mounted");
                default: throw std::runtime_error("unknown or changed local drive type");
            }
        });
        for (const std::string field : {"size", "free", "available"}) {
            tree.RegisterNode(prefix + field, "Volume " + field + " in bytes", [letter, field]() {
                return VolumeSpace(letter, field);
            });
        }
        tree.RegisterNode(prefix + "filesystem", "Volume filesystem name", [letter]() {
            return VolumeInfo(letter, false);
        });
        tree.RegisterNode(prefix + "label", "Volume label", [letter]() {
            return VolumeInfo(letter, true);
        });
    }
    tree.RegisterNode("fs.volume.count", "Number of local drive-letter volumes", [volumes]() {
        return std::to_string(volumes.size());
    });
    tree.RegisterNode("fs.volume.names", "Comma-separated local volume drive letters", [volumeNames]() {
        return volumeNames;
    });
}
