#include "mkfs_fmifs.h"

#include "mkfs_error.h"

#include <stdio.h>
#include <wchar.h>

#define FMIFS_HARDDISK 0x0c

typedef enum MkfsFmifsCommand {
    MKFS_FMIFS_PROGRESS = 0,
    MKFS_FMIFS_DONE_WITH_STRUCTURE = 1,
    MKFS_FMIFS_INSUFFICIENT_RIGHTS = 6,
    MKFS_FMIFS_FILESYSTEM_NOT_SUPPORTED = 7,
    MKFS_FMIFS_VOLUME_IN_USE = 8,
    MKFS_FMIFS_DONE = 11,
    MKFS_FMIFS_STRUCTURE_PROGRESS = 15,
    MKFS_FMIFS_CLUSTER_SIZE_TOO_SMALL = 16
} MkfsFmifsCommand;

typedef BOOLEAN (WINAPI *MkfsFmifsCallback)(MkfsFmifsCommand command, ULONG sub_action,
    PVOID action_info);
typedef VOID (WINAPI *MkfsFormatEx)(PWSTR drive_root, DWORD media_flag, PWSTR file_system,
    PWSTR label, BOOL quick_format, DWORD cluster_size, MkfsFmifsCallback callback);

typedef struct MkfsFormatState {
    MkfsError* error;
    bool verbose;
    bool done;
    bool succeeded;
    bool failed;
} MkfsFormatState;

static MkfsFormatState* active_format;

static BOOLEAN FailFormat(MkfsFormatState* state, DWORD code, const wchar_t* message) {
    if (!state->failed) MkfsErrorSet(state->error, code, message);
    state->failed = true;
    return FALSE;
}

static BOOLEAN WINAPI FormatCallback(MkfsFmifsCommand command, ULONG sub_action, PVOID action_info) {
    (void)sub_action;
    MkfsFormatState* state = active_format;
    if (state == NULL) return FALSE;

    switch (command) {
        case MKFS_FMIFS_PROGRESS:
            if (action_info == NULL) {
                return FailFormat(state, ERROR_INVALID_DATA,
                    L"The formatter returned progress without a percentage");
            }
            wprintf(L"\rFormatting: %3lu%%", (unsigned long)*(const DWORD*)action_info);
            (void)fflush(stdout);
            return TRUE;
        case MKFS_FMIFS_DONE_WITH_STRUCTURE:
            if (state->verbose) wprintf(L"\nFilesystem structures are being written.\n");
            return TRUE;
        case MKFS_FMIFS_DONE:
            if (action_info == NULL) {
                return FailFormat(state, ERROR_INVALID_DATA,
                    L"The formatter returned no completion status");
            }
            state->done = true;
            state->succeeded = *(const BOOLEAN*)action_info != FALSE;
            if (!state->succeeded) {
                return FailFormat(state, ERROR_GEN_FAILURE, L"Windows reported that formatting failed");
            }
            return TRUE;
        case MKFS_FMIFS_INSUFFICIENT_RIGHTS:
            return FailFormat(state, ERROR_ACCESS_DENIED,
                L"Windows denied permission to format the volume");
        case MKFS_FMIFS_FILESYSTEM_NOT_SUPPORTED:
            return FailFormat(state, ERROR_NOT_SUPPORTED,
                L"Windows does not support the requested file system on this volume");
        case MKFS_FMIFS_VOLUME_IN_USE:
            return FailFormat(state, ERROR_BUSY,
                L"The volume is in use and could not be formatted");
        case MKFS_FMIFS_CLUSTER_SIZE_TOO_SMALL:
            return FailFormat(state, ERROR_INVALID_PARAMETER,
                L"The requested cluster size is not valid for this volume");
        case MKFS_FMIFS_STRUCTURE_PROGRESS:
        default:
            return TRUE;
    }
}

static bool GetFmifsPath(wchar_t* path, size_t capacity, MkfsError* error) {
    const UINT length = GetSystemDirectoryW(path, (UINT)capacity);
    if (length == 0 || length >= capacity) {
        const DWORD code = length == 0 ? GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        MkfsErrorSetWindows(error, L"Could not locate the Windows system directory", code);
        return false;
    }
    size_t used = length;
    if (used == 0 || (path[used - 1] != L'\\' && path[used - 1] != L'/')) {
        if (used + 1 >= capacity) {
            MkfsErrorSet(error, ERROR_INSUFFICIENT_BUFFER, L"Windows system directory path is too long");
            return false;
        }
        path[used++] = L'\\';
        path[used] = L'\0';
    }
    if (wcscpy_s(path + used, capacity - used, L"fmifs.dll") != 0) {
        MkfsErrorSet(error, ERROR_INSUFFICIENT_BUFFER, L"Windows system directory path is too long");
        return false;
    }
    return true;
}

static void AppendCleanupError(MkfsError* error, const wchar_t* message, DWORD code) {
    wchar_t combined[MKFS_ERROR_CAPACITY];
    MkfsError cleanup = {0};
    MkfsErrorSetWindows(&cleanup, message, code);
    (void)swprintf_s(combined, _countof(combined),
        L"%.220ls; additionally, %.220ls", error->message, cleanup.message);
    MkfsErrorSet(error, error->code, combined);
}

bool MkfsFormatVolume(const MkfsOptions* options, MkfsError* error) {
    if (options == NULL || error == NULL) {
        MkfsErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid format request");
        return false;
    }
    MkfsErrorClear(error);
    if (active_format != NULL) {
        MkfsErrorSet(error, ERROR_BUSY, L"Another format operation is already active");
        return false;
    }

    wchar_t library_path[MAX_PATH];
    if (!GetFmifsPath(library_path, _countof(library_path), error)) return false;
    HMODULE library = LoadLibraryW(library_path);
    if (library == NULL) {
        MkfsErrorSetWindows(error, L"Could not load the Windows file-system formatter", GetLastError());
        return false;
    }
    MkfsFormatEx format = (MkfsFormatEx)GetProcAddress(library, "FormatEx");
    if (format == NULL) {
        const DWORD code = GetLastError();
        MkfsErrorSetWindows(error, L"Windows file-system formatter does not export FormatEx",
            code == ERROR_SUCCESS ? ERROR_PROC_NOT_FOUND : code);
        if (!FreeLibrary(library)) AppendCleanupError(error, L"Could not unload fmifs.dll", GetLastError());
        return false;
    }

    MkfsFormatState state = {error, options->verbose, false, false, false};
    wchar_t root[] = {options->drive_letter, L':', L'\\', L'\0'};
    wchar_t file_system[8] = {0};
    wchar_t label[MKFS_LABEL_CAPACITY] = {0};
    if (wcscpy_s(file_system, _countof(file_system), MkfsFileSystemName(options->file_system)) != 0 ||
        wcscpy_s(label, _countof(label), options->label) != 0) {
        MkfsErrorSet(error, ERROR_INVALID_PARAMETER, L"Could not prepare format parameters");
        if (!FreeLibrary(library)) AppendCleanupError(error, L"Could not unload fmifs.dll", GetLastError());
        return false;
    }

    active_format = &state;
    format(root, FMIFS_HARDDISK, file_system, label,
        options->quick_format ? TRUE : FALSE, options->cluster_size, FormatCallback);
    active_format = NULL;

    bool success = state.done && state.succeeded && !state.failed;
    if (!success && error->message[0] == L'\0') {
        MkfsErrorSet(error, ERROR_GEN_FAILURE,
            L"Windows formatter returned without a successful completion status");
    }
    if (!FreeLibrary(library)) {
        const DWORD code = GetLastError();
        if (success) {
            MkfsErrorSetWindows(error, L"Formatting completed but fmifs.dll could not be unloaded", code);
        } else {
            AppendCleanupError(error, L"Could not unload fmifs.dll", code);
        }
        success = false;
    }
    return success;
}
