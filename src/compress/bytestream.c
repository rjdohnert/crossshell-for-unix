#include "bytestream.h"

typedef struct {
    IByteStream base;
    HANDLE hFile;
    uint64_t bytes_transferred;
    bool should_close;
    bool error_flag;
} FileStream;

static size_t FileStream_read(IByteStream *super, void *buf, size_t size) {
    FileStream *this = (FileStream *)super;
    if (this->error_flag || this->hFile == INVALID_HANDLE_VALUE) return 0;

    size_t total_read = 0;
    char *p = (char *)buf;

    // Loop to guarantee full packet delivery across fragmented Windows pipes
    while (total_read < size) {
        DWORD bytes_to_read = (DWORD)(size - total_read);
        DWORD bytesRead = 0;
        if (!ReadFile(this->hFile, p + total_read, bytes_to_read, &bytesRead, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_HANDLE_EOF && err != ERROR_BROKEN_PIPE) {
                this->error_flag = true;
            }
            break;
        }
        if (bytesRead == 0) {
            break; // True EOF
        }
        total_read += bytesRead;
    }

    this->bytes_transferred += total_read;
    return total_read;
}

static size_t FileStream_write(IByteStream *super, const void *buf, size_t size) {
    FileStream *this = (FileStream *)super;
    if (this->error_flag || this->hFile == INVALID_HANDLE_VALUE) return 0;

    size_t total_written = 0;
    const char *p = (const char *)buf;

    while (total_written < size) {
        DWORD bytes_to_write = (DWORD)(size - total_written);
        DWORD bytesWritten = 0;
        if (!WriteFile(this->hFile, p + total_written, bytes_to_write, &bytesWritten, NULL) || bytesWritten == 0) {
            this->error_flag = true;
            break;
        }
        total_written += bytesWritten;
    }

    this->bytes_transferred += total_written;
    return total_written;
}

static bool FileStream_flush(IByteStream *super) {
    FileStream *this = (FileStream *)super;
    if (this->hFile != INVALID_HANDLE_VALUE) {
        return FlushFileBuffers(this->hFile) != FALSE;
    }
    return true;
}

static uint64_t FileStream_get_bytes(const IByteStream *super) {
    return ((const FileStream *)super)->bytes_transferred;
}

static bool FileStream_has_error(const IByteStream *super) {
    return ((const FileStream *)super)->error_flag;
}

static void FileStream_destroy(IByteStream *super) {
    FileStream *this = (FileStream *)super;
    if (this) {
        if (this->should_close && this->hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(this->hFile);
        }
        free(this);
    }
}

static const IByteStreamVtbl FileStream_Vtbl = {
    .read = FileStream_read,
    .write = FileStream_write,
    .flush = FileStream_flush,
    .get_bytes_transferred = FileStream_get_bytes,
    .has_error = FileStream_has_error,
    .destroy = FileStream_destroy
};

IByteStream *FileStream_create(const wchar_t *path, DWORD access, DWORD creation) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE h = CreateFileW(path, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    FileStream *stream = (FileStream *)calloc(1, sizeof(FileStream));
    if (!stream) {
        CloseHandle(h);
        return NULL;
    }
    stream->base.vptr = &FileStream_Vtbl;
    stream->hFile = h;
    stream->should_close = true;
    return (IByteStream *)stream;
}

IByteStream *StdioStream_create(DWORD stdHandleType) {
    HANDLE h = GetStdHandle(stdHandleType);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return NULL;

    FileStream *stream = (FileStream *)calloc(1, sizeof(FileStream));
    if (!stream) return NULL;
    stream->base.vptr = &FileStream_Vtbl;
    stream->hFile = h;
    stream->should_close = false;
    return (IByteStream *)stream;
}
