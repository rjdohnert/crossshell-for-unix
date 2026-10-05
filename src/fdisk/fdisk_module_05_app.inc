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

        if (std::wstring_view(argv[arg_idx]) == L"--") {
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
