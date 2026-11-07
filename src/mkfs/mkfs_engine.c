#include "mkfs_engine.h"

#include "mkfs_cli.h"
#include "mkfs_error.h"
#include "mkfs_fmifs.h"

#include <stdio.h>
#include <wchar.h>

static bool ValidateTargetDrive(wchar_t drive_letter, wchar_t* root, MkfsError* error) {
    root[0] = drive_letter;
    root[1] = L':';
    root[2] = L'\\';
    root[3] = L'\0';
    const UINT drive_type = GetDriveTypeW(root);
    if (drive_type == DRIVE_NO_ROOT_DIR || drive_type == DRIVE_UNKNOWN) {
        MkfsErrorSet(error, ERROR_INVALID_DRIVE, L"Target drive does not exist");
        return false;
    }
    if (drive_type == DRIVE_REMOTE || drive_type == DRIVE_CDROM) {
        MkfsErrorSet(error, ERROR_NOT_SUPPORTED,
            L"Only local fixed, removable, or RAM-disk volumes can be formatted");
        return false;
    }
    return true;
}

static bool ConfirmFormat(const MkfsOptions* options, const wchar_t* root, MkfsError* error) {
    if (options->assume_yes) {
        wprintf(L"Confirmation skipped by -y.\n");
        return true;
    }
    wprintf(L"\nWARNING: Formatting %ls as %ls will destroy its existing data.\n",
        root, MkfsFileSystemName(options->file_system));
    wprintf(L"Format mode: %ls. Continue? (y/N): ",
        options->quick_format ? L"quick" : L"full");
    wchar_t response[16];
    if (fgetws(response, (int)_countof(response), stdin) == NULL) {
        MkfsErrorSet(error, ERROR_CANCELLED, L"Formatting cancelled because no confirmation was received");
        return false;
    }
    size_t length = wcslen(response);
    if (length != 0 && response[length - 1] == L'\n') response[--length] = L'\0';
    if (length != 0 && response[length - 1] == L'\r') response[--length] = L'\0';
    if (length >= _countof(response) - 1 && !feof(stdin)) {
        wint_t character;
        while ((character = getwchar()) != WEOF && character != L'\n') {}
        MkfsErrorSet(error, ERROR_INVALID_DATA, L"Confirmation response is too long");
        return false;
    }
    if (length == 1 && (response[0] == L'y' || response[0] == L'Y')) return true;
    MkfsErrorSet(error, ERROR_CANCELLED, L"Formatting cancelled by user");
    return false;
}

static int ReportError(const MkfsError* error) {
    fwprintf(stderr, L"mkfs: %ls\n", error->message);
    if (error->code == ERROR_ACCESS_DENIED || error->code == ERROR_PRIVILEGE_NOT_HELD) {
        fwprintf(stderr, L"mkfs: formatting may require an elevated console.\n");
        return 5;
    }
    return 1;
}

int MkfsRun(int argc, wchar_t* argv[]) {
    MkfsOptions options = {0};
    MkfsError error = {0};
    if (!MkfsOptionsParse(argc, argv, &options, &error)) {
        fwprintf(stderr, L"mkfs: %ls\n", error.message);
        MkfsPrintHelp();
        return 2;
    }
    if (options.show_help) {
        MkfsPrintHelp();
        return 0;
    }
    if (options.show_version) {
        MkfsPrintVersion();
        return 0;
    }

    wchar_t root[4];
    if (!ValidateTargetDrive(options.drive_letter, root, &error)) return ReportError(&error);
    if (options.verbose) {
        wprintf(L"Target: %ls\nFile system: %ls\nFormat mode: %ls\nCluster size: ",
            root, MkfsFileSystemName(options.file_system),
            options.quick_format ? L"quick" : L"full");
        if (options.cluster_size == 0) wprintf(L"Windows default\n");
        else wprintf(L"%lu bytes\n", (unsigned long)options.cluster_size);
        if (options.has_label) wprintf(L"Volume label: %ls\n", options.label);
    }
    if (!ConfirmFormat(&options, root, &error)) {
        if (error.code == ERROR_CANCELLED) {
            wprintf(L"Format cancelled.\n");
            return 3;
        }
        return ReportError(&error);
    }
    if (!MkfsFormatVolume(&options, &error)) return ReportError(&error);
    wprintf(L"\nmkfs: Successfully formatted %ls as %ls.\n",
        root, MkfsFileSystemName(options.file_system));
    return 0;
}
