#include "stream.h"

static int64_t FileStream_Read(IStream* baseSelf, void* buffer, size_t size) {
    FileStream* self = (FileStream*)baseSelf;
    if (self->handle == INVALID_HANDLE_VALUE || self->isEof) return 0;

    size_t totalRead = 0;
    uint8_t* p = (uint8_t*)buffer;

    // Loop to guarantee full reads across fragmented pipes
    while (totalRead < size) {
        DWORD bytesRead = 0;
        if (!ReadFile(self->handle, p + totalRead, (DWORD)(size - totalRead), &bytesRead, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF) {
                self->isEof = true;
                break;
            }
            return -1;
        }
        if (bytesRead == 0) {
            self->isEof = true;
            break;
        }
        totalRead += bytesRead;
    }
    return (int64_t)totalRead;
}

static int64_t FileStream_Write(IStream* baseSelf, const void* buffer, size_t size) {
    FileStream* self = (FileStream*)baseSelf;
    if (self->handle == INVALID_HANDLE_VALUE) return -1;

    DWORD totalWritten = 0;
    while (totalWritten < (DWORD)size) {
        DWORD written = 0;
        if (!WriteFile(self->handle, (const uint8_t*)buffer + totalWritten,
                       (DWORD)size - totalWritten, &written, NULL)) {
            return -1;
        }
        totalWritten += written;
    }
    return (int64_t)totalWritten;
}

static void FileStream_Close(IStream* baseSelf) {
    FileStream* self = (FileStream*)baseSelf;
    if (self->handle != INVALID_HANDLE_VALUE && self->ownsHandle) {
        CloseHandle(self->handle);
        self->handle = INVALID_HANDLE_VALUE;
    }
}

static void FileStream_Destroy(IStream* baseSelf) {
    if (!baseSelf) return;
    FileStream_Close(baseSelf);
    free(baseSelf);
}

static const IStreamVtbl g_FileStreamVtbl = {
    FileStream_Read,
    FileStream_Write,
    FileStream_Close,
    FileStream_Destroy
};

FileStream* FileStream_Create(HANDLE handle, bool ownsHandle) {
    FileStream* self = (FileStream*)calloc(1, sizeof(FileStream));
    if (!self) return NULL;
    self->base.vptr = &g_FileStreamVtbl;
    self->handle = handle;
    self->ownsHandle = ownsHandle;
    return self;
}

FileStream* StdStream_Create(DWORD stdHandleType) {
    HANDLE h = GetStdHandle(stdHandleType);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return NULL;

    if (stdHandleType == STD_INPUT_HANDLE) {
        _setmode(_fileno(stdin), _O_BINARY);
    } else if (stdHandleType == STD_OUTPUT_HANDLE) {
        _setmode(_fileno(stdout), _O_BINARY);
    }

    return FileStream_Create(h, false);
}
