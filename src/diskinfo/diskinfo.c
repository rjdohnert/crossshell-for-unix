// diskinfo 2.0.0 - describe a disk device (HP-UX compatible)
/* CrossShell for UNIX
 * Copyright (C) 2026, PC/OpenSystems LLC
 *
 * Licensed under the terms of the BSD 3-Clause License
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef IOCTL_SCSI_PASS_THROUGH
#define IOCTL_SCSI_PASS_THROUGH         0x0004D004
#define SCSI_IOCTL_DATA_IN              1
#define SCSI_IOCTL_DATA_OUT             0
#define SCSI_IOCTL_DATA_UNSPECIFIED     2

typedef struct _SCSI_PASS_THROUGH {
    USHORT    Length;
    UCHAR     ScsiStatus;
    UCHAR     PathId;
    UCHAR     TargetId;
    UCHAR     Lun;
    UCHAR     CdbLength;
    UCHAR     SenseInfoLength;
    UCHAR     DataIn;
    ULONG     DataTransferLength;
    ULONG     TimeOutValue;
    ULONG_PTR DataBufferOffset;
    ULONG     SenseInfoOffset;
    UCHAR     Cdb[16];
} SCSI_PASS_THROUGH, *PSCSI_PASS_THROUGH;
#endif

typedef struct {
    SCSI_PASS_THROUGH spt;
    ULONG filler;
    UCHAR ucSenseBuf[32];
    UCHAR ucDataBuf[256];
} SCSI_PASS_THROUGH_WITH_BUFFERS;

/* SCSI peripheral device type names matching HP-UX */
static const char *get_device_type(unsigned char dt) {
    switch (dt & 0x1F) {
        case 0x00: return "direct access";
        case 0x01: return "sequential access";
        case 0x02: return "printer";
        case 0x03: return "processor";
        case 0x04: return "write once";
        case 0x05: return "CD-ROM";
        case 0x06: return "scanner";
        case 0x07: return "optical memory";
        case 0x08: return "medium changer";
        case 0x09: return "communications";
        case 0x0C: return "storage array controller";
        case 0x0D: return "enclosure services";
        case 0x0E: return "simplified direct access";
        case 0x0F: return "optical card reader/writer";
        case 0x14: return "zoned block";
        default:   return "direct access";
    }
}

static void trim_copy(char *dst, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    for (int j = (int)i - 1; j >= 0; j--) {
        if (dst[j] == ' ' || dst[j] == '\t' || dst[j] == '\r' || dst[j] == '\n') {
            dst[j] = '\0';
        } else {
            break;
        }
    }
}

/* Normalizes user input: 0 -> \\.\PhysicalDrive0, C: -> \\.\C:, /dev/rdsk/... -> \\.\PhysicalDriveX */
static void normalize_device_path(const char *in, char *out, size_t out_sz) {
    if (strncmp(in, "\\\\.\\", 4) == 0) {
        snprintf(out, out_sz, "%s", in);
        out[out_sz - 1] = '\0';
        return;
    }
    /* HP-UX style naming compatibility */
    if (strstr(in, "disk") || strstr(in, "rdsk")) {
        const char *p = in + strlen(in);
        while (p > in && isdigit((unsigned char)*(p - 1))) p--;
        if (*p) {
            snprintf(out, out_sz, "\\\\.\\PhysicalDrive%s", p);
            return;
        }
    }
    /* Bare disk number: "0", "1" */
    int is_num = 1;
    for (int i = 0; in[i]; i++) {
        if (!isdigit((unsigned char)in[i])) { is_num = 0; break; }
    }
    if (is_num && in[0] != '\0') {
        snprintf(out, out_sz, "\\\\.\\PhysicalDrive%s", in);
        return;
    }
    /* PhysicalDriveX shorthand */
    if (_strnicmp(in, "PhysicalDrive", 13) == 0) {
        snprintf(out, out_sz, "\\\\.\\%s", in);
        return;
    }
    /* Drive letter shorthand: "C:" or "C" */
    if (isalpha((unsigned char)in[0]) && (in[1] == ':' || in[1] == '\0')) {
        snprintf(out, out_sz, "\\\\.\\%c:", in[0]);
        return;
    }
    snprintf(out, out_sz, "\\\\.\\%s", in);
}

static void print_help(const char *prog) {
    printf(
        "diskinfo(1)            CrossShell for UNIX Reference Manual           diskinfo(1)\n"
        "\n"
        "NAME\n"
        "    diskinfo - describe a disk device (HP-UX compatible)\n"
        "\n"
        "SYNOPSIS\n"
        "    %s [-b] [-v] character_devicefile\n"
        "    %s -h\n"
        "\n"
        "DESCRIPTION\n"
        "    Queries a disk device for its vendor, product, type, capacity, and\n"
        "    sector geometry, using a SCSI INQUIRY pass-through when available and\n"
        "    falling back to the Windows storage property query for non-admin use.\n"
        "\n"
        "    The device may be given as a bare disk number (0, 1, ...), a drive\n"
        "    letter (C:), a PhysicalDriveN name, an HP-UX style path\n"
        "    (/dev/rdsk/...), or a raw Windows device path (\\\\.\\...).\n"
        "\n"
        "QUALIFIERS\n"
        "    -b\n"
        "        Print only the disk size in 1024-byte (Kbyte) units and exit.\n"
        "        Useful for scripting.\n"
        "\n"
        "    -v\n"
        "        Verbose output. Additionally prints the revision level, blocks\n"
        "        per disk, ISO/ECMA/ANSI versions, removable-media flag, response\n"
        "        format, and any additional raw inquiry bytes.\n"
        "\n"
        "    -h, -?\n"
        "        Display this reference manual and exit.\n"
        "\n"
        "EXAMPLES\n"
        "    diskinfo 0\n"
        "        Describe the first physical disk.\n"
        "\n"
        "    diskinfo -v C:\n"
        "        Verbosely describe the disk backing drive C:.\n"
        "\n"
        "    diskinfo -b 1\n"
        "        Print the size of the second physical disk in Kbytes.\n"
        "\n"
        "    CrossShell for UNIX                                          diskinfo(1)\n",
        prog, prog
    );
}

static void usage(void) {
    fprintf(stderr, "usage: diskinfo [-b] [-v] character_devicefile\n");
    fprintf(stderr, "Try 'diskinfo -h' for more information.\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int opt_b = 0;
    int opt_v = 0;
    const char *raw_dev_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'b') opt_b = 1;
                else if (argv[i][j] == 'v') opt_v = 1;
                else if (argv[i][j] == 'h' || argv[i][j] == '?') {
                    print_help(argv[0]);
                    return 0;
                }
                else {
                    fprintf(stderr, "diskinfo: illegal option -- %c\n", argv[i][j]);
                    usage();
                }
            }
        } else {
            if (!raw_dev_arg) {
                raw_dev_arg = argv[i];
            } else {
                usage();
            }
        }
    }

    if (!raw_dev_arg) {
        usage();
    }

    char win_path[MAX_PATH];
    normalize_device_path(raw_dev_arg, win_path, sizeof(win_path));

    /* Open device. Try Read/Write first (needed for IOCTL_SCSI_PASS_THROUGH), fall back to Read/Query */
    HANDLE hDevice = CreateFileA(
        win_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        hDevice = CreateFileA(
            win_path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (hDevice == INVALID_HANDLE_VALUE) {
        hDevice = CreateFileA(
            win_path,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "diskinfo: cannot open %s\n", raw_dev_arg);
        return 1;
    }

    /* 1. Retrieve capacity and sector size */
    unsigned long long disk_size_bytes = 0;
    unsigned long bytes_per_sector = 512;
    DWORD bytes_returned = 0;

    DISK_GEOMETRY_EX geom_ex;
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
                        &geom_ex, sizeof(geom_ex), &bytes_returned, NULL)) {
        disk_size_bytes = (unsigned long long)geom_ex.DiskSize.QuadPart;
        bytes_per_sector = geom_ex.Geometry.BytesPerSector;
    } else {
        DISK_GEOMETRY geom;
        if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                            &geom, sizeof(geom), &bytes_returned, NULL)) {
            bytes_per_sector = geom.BytesPerSector;
            disk_size_bytes = (unsigned long long)geom.Cylinders.QuadPart *
                              geom.TracksPerCylinder *
                              geom.SectorsPerTrack *
                              geom.BytesPerSector;
        } else {
            GET_LENGTH_INFORMATION len_info;
            if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                                &len_info, sizeof(len_info), &bytes_returned, NULL)) {
                disk_size_bytes = (unsigned long long)len_info.Length.QuadPart;
            }
        }
    }

    if (bytes_per_sector == 0) bytes_per_sector = 512;
    unsigned long long size_kbytes = disk_size_bytes / 1024;
    unsigned long long blocks_per_disk = disk_size_bytes / bytes_per_sector;

    /* Qualifier -b: Print size in 1024-byte units and exit */
    if (opt_b) {
        printf("%llu\n", size_kbytes);
        CloseHandle(hDevice);
        return 0;
    }

    /* 2. Retrieve SCSI Inquiry Data */
    unsigned char inq_buf[256];
    memset(inq_buf, 0, sizeof(inq_buf));
    int has_raw_inquiry = 0;
    int inq_total_len = 0;

    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;
    memset(&sptwb, 0, sizeof(sptwb));
    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);
    sptwb.spt.CdbLength = 6;
    sptwb.spt.SenseInfoLength = sizeof(sptwb.ucSenseBuf);
    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sptwb.spt.DataTransferLength = sizeof(sptwb.ucDataBuf);
    sptwb.spt.TimeOutValue = 5;
    sptwb.spt.DataBufferOffset = (ULONG_PTR)(&sptwb.ucDataBuf[0]) - (ULONG_PTR)(&sptwb);
    sptwb.spt.SenseInfoOffset  = (ULONG_PTR)(&sptwb.ucSenseBuf[0]) - (ULONG_PTR)(&sptwb);
    sptwb.spt.Cdb[0] = 0x12; /* SCSI INQUIRY */
    sptwb.spt.Cdb[4] = (UCHAR)(sizeof(sptwb.ucDataBuf) - 1);

    if (DeviceIoControl(hDevice, IOCTL_SCSI_PASS_THROUGH, &sptwb, sizeof(sptwb),
                        &sptwb, sizeof(sptwb), &bytes_returned, NULL) && sptwb.spt.ScsiStatus == 0) {
        memcpy(inq_buf, sptwb.ucDataBuf, sizeof(inq_buf));
        inq_total_len = 5 + inq_buf[4];
        if (inq_total_len > (int)sizeof(inq_buf)) inq_total_len = (int)sizeof(inq_buf);
        has_raw_inquiry = 1;
    }

    /* Fallback to Storage Query Property (works for non-admin accounts) */
    char vendor_str[64] = "Generic";
    char product_str[64] = "Storage Device";
    char rev_str[32] = "1.00";
    unsigned char dev_type = 0x00;
    int removable = 0;
    int iso_ver = 0, ecma_ver = 0, ansi_ver = 3, resp_fmt = 2;

    if (has_raw_inquiry) {
        dev_type = inq_buf[0] & 0x1F;
        removable = (inq_buf[1] & 0x80) ? 1 : 0;
        iso_ver = (inq_buf[2] >> 6) & 0x03;
        ecma_ver = (inq_buf[2] >> 3) & 0x07;
        ansi_ver = inq_buf[2] & 0x07;
        resp_fmt = inq_buf[3] & 0x0F;

        trim_copy(vendor_str, (char *)&inq_buf[8], 8);
        trim_copy(product_str, (char *)&inq_buf[16], 16);
        trim_copy(rev_str, (char *)&inq_buf[32], 4);
    } else {
        BYTE prop_buf[4096];
        STORAGE_PROPERTY_QUERY query;
        memset(&query, 0, sizeof(query));
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                            prop_buf, sizeof(prop_buf), &bytes_returned, NULL)) {
            STORAGE_DEVICE_DESCRIPTOR *desc = (STORAGE_DEVICE_DESCRIPTOR *)prop_buf;
            dev_type = desc->DeviceType;
            removable = desc->RemovableMedia ? 1 : 0;

            if (desc->VendorIdOffset && prop_buf[desc->VendorIdOffset])
                trim_copy(vendor_str, (char *)(prop_buf + desc->VendorIdOffset), 32);
            if (desc->ProductIdOffset && prop_buf[desc->ProductIdOffset])
                trim_copy(product_str, (char *)(prop_buf + desc->ProductIdOffset), 32);
            if (desc->ProductRevisionOffset && prop_buf[desc->ProductRevisionOffset])
                trim_copy(rev_str, (char *)(prop_buf + desc->ProductRevisionOffset), 16);

            if (desc->RawPropertiesLength >= 36) {
                memcpy(inq_buf, desc->RawDeviceProperties,
                       min(desc->RawPropertiesLength, (DWORD)sizeof(inq_buf)));
                inq_total_len = 5 + inq_buf[4];
                if (inq_total_len > (int)sizeof(inq_buf)) inq_total_len = (int)sizeof(inq_buf);
                has_raw_inquiry = 1;
            }
        }
    }

    CloseHandle(hDevice);

    /* 3. Output - Exact format (19-character right-aligned labels) */
    printf("Disk Information: %s:\n", raw_dev_arg);
    printf("%19s: %s\n", "vendor", vendor_str);
    printf("%19s: %s\n", "product id", product_str);
    printf("%19s: %s\n", "type", get_device_type(dev_type));
    printf("%19s: %llu Kbytes\n", "size", size_kbytes);
    printf("%19s: %lu\n", "bytes per sector", bytes_per_sector);

    if (opt_v) {
        printf("%19s: %s\n", "rev level", rev_str);
        printf("%19s: %llu\n", "blocks per disk", blocks_per_disk);
        printf("%19s: %d\n", "ISO version", iso_ver);
        printf("%19s: %d\n", "ECMA version", ecma_ver);
        printf("%19s: %d\n", "ANSI version", ansi_ver);
        printf("%19s: %s\n", "removable media", removable ? "yes" : "no");
        printf("%19s: %d\n", "response format", resp_fmt);

        if (has_raw_inquiry && inq_total_len > 32) {
            printf("(Additional inquiry bytes: ");
            for (int i = 32; i < inq_total_len; i++) {
                printf("(%d)%x ", i, inq_buf[i]);
            }
            printf(")\n");
        }
    }

    return 0;
}