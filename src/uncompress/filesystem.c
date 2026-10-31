#include "filesystem.h"

wchar_t g_ActiveTempPath[PATH_BUFFER_CAPACITY] = { 0 };
HANDLE  g_ActiveTempHandle = INVALID_HANDLE_VALUE;

static BOOL WINAPI SignalHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            g_Interrupted = true;
            if (g_ActiveTempHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(g_ActiveTempHandle);
                g_ActiveTempHandle = INVALID_HANDLE_VALUE;
            }
            if (g_ActiveTempPath[0] != L'\0') {
                DeleteFileW(g_ActiveTempPath);
                g_ActiveTempPath[0] = L'\0';
            }
            return TRUE;
        default:
            return FALSE;
    }
}

FileSystemManager* FileSystemManager_Create(bool force, bool keep, bool quiet) {
    FileSystemManager* self = (FileSystemManager*)calloc(1, sizeof(FileSystemManager));
    if (!self) return NULL;
    self->force = force;
    self->keep = keep;
    self->quiet = quiet;
    SetConsoleCtrlHandler(SignalHandler, TRUE);
    return self;
}

void FileSystemManager_Destroy(FileSystemManager* self) {
    free(self);
}

bool FileSystemManager_HasZSuffix(const wchar_t* path) {
    size_t len = wcslen(path);
    if (len < 2) return false;
    return (_wcsicmp(path + len - 2, L".Z") == 0);
}

bool FileSystemManager_PromptOverwrite(const wchar_t* targetPath) {
    if (!_isatty(_fileno(stdin))) return false;

    fwprintf(stderr, L"%ls already exists; do you wish to overwrite (y or n)? ", targetPath);
    fflush(stderr);

    wchar_t response[64];
    if (!fgetws(response, 64, stdin)) return false;
    return (response[0] == L'y' || response[0] == L'Y');
}

void FileSystemManager_PreserveTimestamps(HANDLE hSrc, HANDLE hDst) {
    FILETIME ftCreate, ftAccess, ftWrite;
    if (GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite)) {
        SetFileTime(hDst, &ftCreate, &ftAccess, &ftWrite);
    }
}
