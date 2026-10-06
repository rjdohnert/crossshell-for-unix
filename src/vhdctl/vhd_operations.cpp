#include "scoped_virtual_disk_handle.hpp"
#include "vhd_operations.hpp"
#include "vhd_reporter.hpp"
#include "vhd_storage_inspector.hpp"

bool VhdOperations::CreateDisk(const std::wstring& path, ULONGLONG sizeInBytes, bool isFixed, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        if (storageType.DeviceId == VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN) {
            storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
        }
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        CREATE_VIRTUAL_DISK_PARAMETERS params = {};
        params.Version = CREATE_VIRTUAL_DISK_VERSION_2;
        params.Version2.UniqueId = GUID{};
        params.Version2.MaximumSize = sizeInBytes;
        params.Version2.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE;
        params.Version2.SectorSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_SECTOR_SIZE;
        params.Version2.ParentPath = NULL;
        params.Version2.SourcePath = NULL;

        CREATE_VIRTUAL_DISK_FLAG flags = CREATE_VIRTUAL_DISK_FLAG_NONE;
        if (isFixed) {
            flags |= CREATE_VIRTUAL_DISK_FLAG_FULL_PHYSICAL_ALLOCATION;
        }

        ScopedVirtualDiskHandle hVhd;
        if (format == OutputFormat::Human) {
            std::wcout << L"vhdctl: Creating " << (isFixed ? L"fixed" : L"dynamic")
                       << L" virtual disk (" << (sizeInBytes / (1024 * 1024)) << L" MB)..." << std::endl;
        }

        DWORD result = CreateVirtualDisk(
            &storageType,
            path.c_str(),
            VIRTUAL_DISK_ACCESS_NONE,
            NULL,
            flags,
            0,
            &params,
            NULL,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error creating disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"create", path, L"Virtual disk created successfully");
        return true;
    }

bool VhdOperations::MountDisk(const std::wstring& path, bool readOnly, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
        openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;

        VIRTUAL_DISK_ACCESS_MASK accessMask = VIRTUAL_DISK_ACCESS_ATTACH_RO | VIRTUAL_DISK_ACCESS_GET_INFO;
        if (!readOnly) {
            accessMask |= VIRTUAL_DISK_ACCESS_ATTACH_RW;
        }

        ScopedVirtualDiskHandle hVhd;
        DWORD result = OpenVirtualDisk(
            &storageType,
            path.c_str(),
            accessMask,
            OPEN_VIRTUAL_DISK_FLAG_NONE,
            &openParams,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error opening disk file: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        ATTACH_VIRTUAL_DISK_PARAMETERS attachParams = {};
        attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

        ATTACH_VIRTUAL_DISK_FLAG attachFlags = ATTACH_VIRTUAL_DISK_FLAG_NONE;
        if (readOnly) {
            attachFlags |= ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY;
        }

        result = AttachVirtualDisk(
            hVhd.Get(),
            NULL,
            attachFlags,
            0,
            &attachParams,
            NULL
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error mounting disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"mount", path, readOnly ? L"Virtual disk mounted successfully [Read-Only]" : L"Virtual disk mounted successfully [Read-Write]");
        return true;
    }

bool VhdOperations::UnmountDisk(const std::wstring& path, OutputFormat format) {
        VIRTUAL_STORAGE_TYPE storageType = {};
        storageType.DeviceId = VhdStorageInspector::GetDeviceType(path);
        storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

        OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
        openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;

        ScopedVirtualDiskHandle hVhd;
        DWORD result = OpenVirtualDisk(
            &storageType,
            path.c_str(),
            VIRTUAL_DISK_ACCESS_DETACH,
            OPEN_VIRTUAL_DISK_FLAG_NONE,
            &openParams,
            hVhd.AddressOf()
        );

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error opening mounted disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        result = DetachVirtualDisk(hVhd.Get(), DETACH_VIRTUAL_DISK_FLAG_NONE, 0);

        if (result != ERROR_SUCCESS) {
            std::wcerr << L"vhdctl: Error unmounting disk: " << VhdStorageInspector::GetErrorMessage(result) << std::endl;
            return false;
        }

        VhdReporter::EmitResult(format, L"unmount", path, L"Virtual disk unmounted successfully");
        return true;
    }
