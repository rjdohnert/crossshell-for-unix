#include "metadata.h"

bool FileMetadata_capture(FileMetadata *this, const wchar_t *src_path) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE h = CreateFileW(src_path, GENERIC_READ, share, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        this->valid = false;
        return false;
    }
    this->valid = GetFileTime(h, &this->ftCreation, &this->ftLastAccess, &this->ftLastWrite) != FALSE;
    CloseHandle(h);
    this->dwAttributes = GetFileAttributesW(src_path);
    return this->valid;
}

bool FileMetadata_apply(const FileMetadata *this, const wchar_t *dst_path) {
    if (!this->valid) return false;

    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE h = CreateFileW(dst_path, FILE_WRITE_ATTRIBUTES, share, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFileTime(h, &this->ftCreation, &this->ftLastAccess, &this->ftLastWrite);
        CloseHandle(h);
    }
    if (this->dwAttributes != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(dst_path, this->dwAttributes);
    }
    return true;
}
