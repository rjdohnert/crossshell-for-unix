#include "fdisk_app.h"

#include "fdisk_device.h"
#include "fdisk_editor.h"
#include "fdisk_error.h"
#include "fdisk_format.h"
#include "fdisk_image.h"
#include "fdisk_options.h"

#include <stdio.h>
#include <stdlib.h>

static int ReportDiskError(uint32_t number, const FdiskError* error) {
    fwprintf(stderr, L"fdisk: PhysicalDrive%lu: %ls\n",
        (unsigned long)number, error->message);
    if (error->code == ERROR_ACCESS_DENIED) {
        fwprintf(stderr, L"fdisk: access was denied; try an elevated console if this disk requires it.\n");
        return 5;
    }
    return 1;
}

static int ReportImageError(const FdiskError* error) {
    fwprintf(stderr, L"fdisk: %ls\n", error->message);
    if (error->code == ERROR_ACCESS_DENIED || error->code == ERROR_PRIVILEGE_NOT_HELD) {
        fwprintf(stderr, L"fdisk: virtual disk access was denied; try an elevated console.\n");
        return 5;
    }
    return 1;
}

static int InspectDisk(uint32_t number, bool sector_units) {
    FdiskDisk disk = {0};
    FdiskError error = {0};
    if (!FdiskReadDisk(number, &disk, &error)) return ReportDiskError(number, &error);

    const bool printed = FdiskPrintDisk(&disk, sector_units, &error);
    FdiskFreeDisk(&disk);
    if (!printed) {
        fwprintf(stderr, L"fdisk: %ls\n", error.message);
        return 1;
    }
    return 0;
}

int FdiskRun(int argc, wchar_t* argv[]) {
    FdiskOptions options = {0};
    FdiskError error = {0};
    if (!FdiskParseOptions(argc, argv, &options, &error)) {
        fwprintf(stderr, L"fdisk: %ls\n", error.message);
        FdiskPrintUsage();
        return 2;
    }

    if (options.show_help) {
        FdiskPrintUsage();
        return 0;
    }
    if (options.show_version) {
        FdiskPrintVersion();
        return 0;
    }
    if (options.image_action == FDISK_IMAGE_ACTION_CREATE) {
        if (!FdiskImageCreate(options.image_path, options.image_format,
                options.image_size_bytes, &error)) return ReportImageError(&error);
        wprintf(L"Created %llu MiB virtual disk image: %ls\n",
            (unsigned long long)(options.image_size_bytes / (1024u * 1024u)),
            options.image_path);
        return 0;
    }
    if (options.image_action == FDISK_IMAGE_ACTION_RESIZE) {
        if (!FdiskImageResize(options.image_path, options.image_size_bytes, &error))
            return ReportImageError(&error);
        wprintf(L"Resized virtual disk image to %llu MiB: %ls\n",
            (unsigned long long)(options.image_size_bytes / (1024u * 1024u)),
            options.image_path);
        return 0;
    }
    if (options.image_action == FDISK_IMAGE_ACTION_DETACH) {
        if (!FdiskImageDetach(options.image_path, &error)) return ReportImageError(&error);
        wprintf(L"Detached virtual disk image: %ls\n", options.image_path);
        return 0;
    }
    if (options.image_action == FDISK_IMAGE_ACTION_USE) {
        FdiskImageSession session = {0};
        if (!FdiskImageAttach(options.image_path, &session, &error))
            return ReportImageError(&error);
        const int result = options.list_all
            ? InspectDisk(session.disk_number, options.sector_units)
            : FdiskEditDisk(session.disk_number, options.sector_units);
        const bool handle_closed = FdiskImageCloseSession(&session, &error);
        wprintf(L"Image remains attached as PhysicalDrive%lu; use --detach-image to detach it.\n",
            (unsigned long)session.disk_number);
        if (!handle_closed) {
            fwprintf(stderr, L"fdisk: %ls\n", error.message);
            return result == 0 ? 1 : result;
        }
        return result;
    }
    if (options.has_disk && !options.list_all) {
        return FdiskEditDisk(options.disk_number, options.sector_units);
    }
    if (options.has_disk) return InspectDisk(options.disk_number, options.sector_units);

    uint32_t* numbers = NULL;
    size_t count = 0;
    if (!FdiskEnumerateDiskNumbers(&numbers, &count, &error)) {
        fwprintf(stderr, L"fdisk: %ls\n", error.message);
        return 1;
    }
    if (count == 0) {
        fwprintf(stderr, L"fdisk: no physical disk interfaces were found.\n");
        free(numbers);
        return 1;
    }

    int result = 0;
    for (size_t i = 0; i < count; ++i) {
        const int disk_result = InspectDisk(numbers[i], options.sector_units);
        if (disk_result != 0) result = disk_result;
    }
    free(numbers);
    return result;
}
