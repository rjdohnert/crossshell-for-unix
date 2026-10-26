#include "ls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static wchar_t FileRecord_GetClassifySuffix(const IFileRecord *self, bool slash_only) {
    if (self->attributes & FILE_ATTRIBUTE_DIRECTORY) return L'/';
    if (slash_only) return L'\0';
    if (self->attributes & FILE_ATTRIBUTE_REPARSE_POINT) return L'@';
    const wchar_t *ext = wcsrchr(self->file_name, L'.');
    if (ext && (_wcsicmp(ext, L".exe") == 0 || _wcsicmp(ext, L".cmd") == 0 ||
                _wcsicmp(ext, L".bat") == 0 || _wcsicmp(ext, L".ps1") == 0)) return L'*';
    return L'\0';
}

static void FileRecord_FormatWindowsAttributes(const IFileRecord *self, wchar_t out[10]) {
    out[0] = (self->attributes & FILE_ATTRIBUTE_DIRECTORY) ? L'd' : L'-';
    out[1] = (self->attributes & FILE_ATTRIBUTE_READONLY) ? L'r' : L'-';
    out[2] = (self->attributes & FILE_ATTRIBUTE_ARCHIVE) ? L'a' : L'-';
    out[3] = (self->attributes & FILE_ATTRIBUTE_HIDDEN) ? L'h' : L'-';
    out[4] = (self->attributes & FILE_ATTRIBUTE_SYSTEM) ? L's' : L'-';
    out[5] = (self->attributes & FILE_ATTRIBUTE_COMPRESSED) ? L'c' : L'-';
    out[6] = (self->attributes & FILE_ATTRIBUTE_ENCRYPTED) ? L'e' : L'-';
    out[7] = (self->attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? L'l' : L'-';
    out[8] = self->dacl_state == LS_DACL_PRESENT ? L'+' :
             self->dacl_state == LS_DACL_ABSENT ? L'-' : L'?';
    out[9] = L'\0';
}

static ULONGLONG FileRecord_GetBlockCount(const IFileRecord *self, size_t block_size) {
    if (block_size == 0) block_size = DEFAULT_BLOCK_SIZE;
    return (self->allocation_size.QuadPart + block_size - 1) / block_size;
}

static time_t FileRecord_GetSelectedTime(const IFileRecord *self, const LsConfig *cfg) {
    const FILETIME *file_time = &self->ft_last_write;
    if (cfg->use_access_time) file_time = &self->ft_last_access;
    if (cfg->use_create_time) file_time = &self->ft_creation;
    ULARGE_INTEGER value;
    value.LowPart = file_time->dwLowDateTime;
    value.HighPart = file_time->dwHighDateTime;
    return (time_t)((value.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static void FileRecord_Destroy(IFileRecord *self) {
    free(self->file_name);
    free(self->full_path);
    free(self->acl_entries);
}

IFileRecord FileRecord_Create(const WIN32_FIND_DATAW *wfd, const wchar_t *parent_dir,
                              const wchar_t *full_path_override,
                              ISecurityContext *sec_ctx, const LsConfig *cfg,
                              bool command_line_target) {
    IFileRecord record;
    memset(&record, 0, sizeof(record));
    record.file_name = _wcsdup(wfd->cFileName);
    if (full_path_override) {
        record.full_path = _wcsdup(full_path_override);
    } else {
        size_t full_len = wcslen(parent_dir) + wcslen(wfd->cFileName) + 2;
        record.full_path = (wchar_t *)malloc(full_len * sizeof(wchar_t));
        if (record.full_path) {
            _snwprintf_s(record.full_path, full_len, _TRUNCATE,
                         L"%s\\%s", parent_dir, wfd->cFileName);
        }
    }
    record.attributes = wfd->dwFileAttributes;
    record.reparse_tag = (record.attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? wfd->dwReserved0 : 0;
    record.file_size.LowPart = wfd->nFileSizeLow;
    record.file_size.HighPart = wfd->nFileSizeHigh;
    record.ft_creation = wfd->ftCreationTime;
    record.ft_last_access = wfd->ftLastAccessTime;
    record.ft_last_write = wfd->ftLastWriteTime;
    record.link_count = 1;
    record.allocation_size.QuadPart = (record.file_size.QuadPart + 4095) & ~4095ULL;
    if (record.file_size.QuadPart > 0 && record.allocation_size.QuadPart == 0) {
        record.allocation_size.QuadPart = 4096;
    }

    if (cfg->long_format || cfg->show_acl || cfg->show_blocks || cfg->show_inode) {
        DWORD desired_access = FILE_READ_ATTRIBUTES;
        if (cfg->long_format || cfg->show_acl) desired_access |= READ_CONTROL;
        bool follow_link = cfg->follow_all_symlinks ||
                   (cfg->follow_command_line_symlinks && command_line_target);
        DWORD open_flags = FILE_FLAG_BACKUP_SEMANTICS;
        if (!follow_link) open_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
        HANDLE file = CreateFileW(record.full_path, desired_access,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL, OPEN_EXISTING, open_flags, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info;
            if (GetFileInformationByHandle(file, &info)) {
                if (follow_link) {
                    record.attributes = info.dwFileAttributes;
                    record.reparse_tag = 0;
                    record.file_size.LowPart = info.nFileSizeLow;
                    record.file_size.HighPart = info.nFileSizeHigh;
                    record.ft_creation = info.ftCreationTime;
                    record.ft_last_access = info.ftLastAccessTime;
                    record.ft_last_write = info.ftLastWriteTime;
                }
                record.link_count = info.nNumberOfLinks;
                record.file_id = ((ULONGLONG)info.nFileIndexHigh << 32) | info.nFileIndexLow;
                record.volume_serial = info.dwVolumeSerialNumber;
                record.file_id_valid = true;
            }
            FILE_STANDARD_INFO standard_info;
            if (GetFileInformationByHandleEx(file, FileStandardInfo, &standard_info,
                                             sizeof(standard_info)) &&
                standard_info.AllocationSize.QuadPart >= 0) {
                record.allocation_size.QuadPart = (ULONGLONG)standard_info.AllocationSize.QuadPart;
            }
            if (cfg->long_format || cfg->show_acl) {
                sec_ctx->InspectAcl(sec_ctx, file, record.owner, 128,
                                    record.group, 128, &record.dacl_state,
                                    cfg->show_acl, &record.acl_entries);
            }
            CloseHandle(file);
        } else {
            if (cfg->long_format || cfg->show_acl) {
                wcsncpy_s(record.owner, 128, L"UNKNOWN", _TRUNCATE);
                wcsncpy_s(record.group, 128, L"UNKNOWN", _TRUNCATE);
            }
            if (cfg->show_acl) {
                record.acl_entries = _wcsdup(L"DACL unavailable (cannot open file security descriptor)\n");
            }
        }
    }
    FileRecord_FormatWindowsAttributes(&record, record.windows_attributes);
    record.GetClassifySuffix = FileRecord_GetClassifySuffix;
    record.GetBlockCount = FileRecord_GetBlockCount;
    record.GetSelectedTime = FileRecord_GetSelectedTime;
    record.Destroy = FileRecord_Destroy;
    return record;
}