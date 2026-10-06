#include "vhd_reporter.hpp"
#include "vhd_storage_inspector.hpp"

void VhdReporter::EmitResult(OutputFormat format, const std::wstring& action, const std::wstring& path, const std::wstring& detail) {
        if (format == OutputFormat::Json) {
            std::wcout << L"{\"action\":\"" << action << L"\",\"path\":\"" << path << L"\",\"detail\":\"" << detail << L"\"}\n";
        } else if (format == OutputFormat::Csv) {
            std::wcout << VhdStorageInspector::CsvQuote(action) << L"," << VhdStorageInspector::CsvQuote(path) << L"," << VhdStorageInspector::CsvQuote(detail) << L"\n";
        } else if (format == OutputFormat::Table) {
            std::wcout << L"ACTION\tPATH\tDETAIL\n" << action << L"\t" << path << L"\t" << detail << L"\n";
        } else {
            std::wcout << L"vhdctl: " << detail << L": " << path << L"\n";
        }
    }

void VhdReporter::PrintUsage() {
        std::wcout << LR"(vhdctl(1)               CrossShell for UNIX Reference Manual                 vhdctl(1)

    NAME
        vhdctl - create, mount, and manage VHD and VHDX virtual disk images

    SYNOPSIS
        vhdctl COMMAND [OPTIONS]

    DESCRIPTION
        Creates, mounts, and unmounts Virtual Hard Disk (VHD and VHDX) container
        files using the Windows VirtDisk API subsystem. Requires administrative
        privileges to attach and detach disk images in the Windows storage stack.

    COMMANDS
        create, cr
            Create a new fixed or dynamically expanding virtual disk file.

        mount, attach
            Attach a virtual disk image to the system storage hierarchy.

        unmount, detach
            Detach an active virtual disk image from the system.

    OPTIONS
        -f, --file PATH
            Specify the target .vhd or .vhdx disk image file path.

        -s, --size SIZE
            Specify the maximum virtual disk capacity (e.g., 500M, 10G, 1T).
            Required for create.

        -t, --type TYPE
            Set allocation type: dynamic (sparse/expanding) or fixed
            (fully allocated). Default is dynamic.

        -r, --readonly
            Attach the virtual disk in read-only mode (mount only).

        --json, --csv, --table
            Format execution summary output as JSON, CSV, or an aligned table.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        vhdctl create -f C:\Disks\storage.vhdx -s 50G -t dynamic
            Create a 50 GB dynamically expanding VHDX disk image.

        vhdctl mount -f C:\Disks\storage.vhdx
            Attach a virtual disk with read-write access.

        vhdctl mount -f C:\Disks\backup.vhd -r
            Mount a virtual disk in read-only mode.

        vhdctl unmount -f C:\Disks\storage.vhdx
            Detach the virtual disk from the system.

        vhdctl create -f D:\data.vhdx -s 100G --json
            Create a virtual disk and emit execution details as JSON.

    CrossShell for UNIX                                                     vhdctl(1)
)";
    }

void VhdReporter::PrintVersion() {
        std::wcout << L"vhdctl v1.0.0\n";
    }
