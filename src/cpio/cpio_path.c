#include "cpio_path.h"

/* ========================================================================= */
/* UNICODE / UTF-8 & WIN32 EXTENDED PATHS                                    */
/* ========================================================================= */

wchar_t* Utf8ToWide(const char* utf8) {
    if (!utf8) return NULL;
    int req = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (req <= 0) {
        req = MultiByteToWideChar(CP_ACP, 0, utf8, -1, NULL, 0);
        if (req <= 0) return NULL;
        wchar_t* wstr = (wchar_t*)malloc(req * sizeof(wchar_t));
        if (wstr) MultiByteToWideChar(CP_ACP, 0, utf8, -1, wstr, req);
        return wstr;
    }
    wchar_t* wstr = (wchar_t*)malloc(req * sizeof(wchar_t));
    if (wstr) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, req);
    return wstr;
}

char* WideToUtf8(const wchar_t* wide) {
    if (!wide) return NULL;
    int req = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (req <= 0) return NULL;
    char* utf8 = (char*)malloc(req);
    if (utf8) WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, req, NULL, NULL);
    return utf8;
}

/**
 * Resolves full canonical paths and prepends \\?\ for deep trees (>= 240 chars).
 */
wchar_t* PrepareWin32Path(const wchar_t* wpath) {
    if (!wpath) return NULL;
    if (wcsncmp(wpath, L"\\\\?\\", 4) == 0) return _wcsdup(wpath);

    DWORD req = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (req == 0) return _wcsdup(wpath);

    wchar_t* full = (wchar_t*)malloc(req * sizeof(wchar_t));
    if (!full) return _wcsdup(wpath);
    GetFullPathNameW(wpath, req, full, NULL);

    if (req >= 240 && iswalpha(full[0]) && full[1] == L':') {
        wchar_t* prep = (wchar_t*)malloc((req + 5) * sizeof(wchar_t));
        if (prep) {
            swprintf(prep, req + 5, L"\\\\?\\%s", full);
            free(full);
            return prep;
        }
    }
    return full;
}

FILE* FileOpenUtf8(const char* path, const char* mode) {
    wchar_t* wpath = Utf8ToWide(path);
    wchar_t* wmode = Utf8ToWide(mode);
    if (!wpath || !wmode) {
        free(wpath);
        free(wmode);
        return NULL;
    }
    wchar_t* prep = PrepareWin32Path(wpath);
    FILE* fp = _wfopen(prep ? prep : wpath, wmode);
    free(prep);
    free(wpath);
    free(wmode);
    return fp;
}

void NormalizeSlashes(char* path) {
    for (char* p = path; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
}

int Stat64Utf8(const char* path, struct _stat64* st) {
    char* clean = _strdup(path);
    if (!clean) return -1;
    NormalizeSlashes(clean);

    size_t len = strlen(clean);
    while (len > 1 && clean[len - 1] == '/') {
        if (len == 3 && isalpha((unsigned char)clean[0]) && clean[1] == ':') {
            break;
        }
        clean[--len] = '\0';
    }

    wchar_t* wpath = Utf8ToWide(clean);
    free(clean);
    if (!wpath) return -1;

    wchar_t* prep = PrepareWin32Path(wpath);
    int ret = _wstat64(prep ? prep : wpath, st);
    free(prep);
    free(wpath);
    return ret;
}

int MakeDirUtf8(const char* path) {
    wchar_t* wpath = Utf8ToWide(path);
    if (!wpath) return -1;
    wchar_t* prep = PrepareWin32Path(wpath);
    int ret = _wmkdir(prep ? prep : wpath);
    free(prep);
    free(wpath);
    return ret;
}

int UnlinkUtf8(const char* path) {
    wchar_t* wpath = Utf8ToWide(path);
    if (!wpath) return -1;
    wchar_t* prep = PrepareWin32Path(wpath);
    int ret = _wunlink(prep ? prep : wpath);
    free(prep);
    free(wpath);
    return ret;
}

DWORD GetFileAttrUtf8(const char* path) {
    wchar_t* wpath = Utf8ToWide(path);
    if (!wpath) return INVALID_FILE_ATTRIBUTES;
    wchar_t* prep = PrepareWin32Path(wpath);
    DWORD ret = GetFileAttributesW(prep ? prep : wpath);
    free(prep);
    free(wpath);
    return ret;
}

BOOL SetFileAttrUtf8(const char* path, DWORD attr) {
    wchar_t* wpath = Utf8ToWide(path);
    if (!wpath) return FALSE;
    wchar_t* prep = PrepareWin32Path(wpath);
    BOOL ret = SetFileAttributesW(prep ? prep : wpath, attr);
    free(prep);
    free(wpath);
    return ret;
}

int Utime64Utf8(const char* path, struct __utimbuf64* ut) {
    wchar_t* wpath = Utf8ToWide(path);
    if (!wpath) return -1;
    wchar_t* prep = PrepareWin32Path(wpath);
    int ret = _wutime64(prep ? prep : wpath, ut);
    free(prep);
    free(wpath);
    return ret;
}

bool CreateHardLinkUtf8(const char* targetPath, const char* existingPath) {
    wchar_t* wTarget = Utf8ToWide(targetPath);
    wchar_t* wExisting = Utf8ToWide(existingPath);
    if (!wTarget || !wExisting) {
        free(wTarget);
        free(wExisting);
        return false;
    }
    wchar_t* pTarget = PrepareWin32Path(wTarget);
    wchar_t* pExisting = PrepareWin32Path(wExisting);

    BOOL ret = CreateHardLinkW(pTarget ? pTarget : wTarget,
                               pExisting ? pExisting : wExisting, NULL);
    free(pTarget);
    free(pExisting);
    free(wTarget);
    free(wExisting);
    return ret != 0;
}

bool CreateSymlinkUtf8(const char* targetPath, const char* symlinkTarget, bool isDir) {
    wchar_t* wTarget = Utf8ToWide(targetPath);
    wchar_t* wLink = Utf8ToWide(symlinkTarget);
    if (!wTarget || !wLink) {
        free(wTarget);
        free(wLink);
        return false;
    }
    wchar_t* pTarget = PrepareWin32Path(wTarget);

    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (isDir) flags |= 0x1; /* SYMBOLIC_LINK_FLAG_DIRECTORY */

    BOOL ret = CreateSymbolicLinkW(pTarget ? pTarget : wTarget, wLink, flags);
    free(pTarget);
    free(wTarget);
    free(wLink);
    return ret != 0;
}

bool CopyFileFallback(const char* src, const char* dst) {
    FILE* in = FileOpenUtf8(src, "rb");
    if (!in) return false;
    FILE* out = FileOpenUtf8(dst, "wb");
    if (!out) { fclose(in); return false; }

    char buf[BUFFER_CHUNK_SIZE];
    size_t r;
    bool ok = true;
    while ((r = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, r, out) != r) { ok = false; break; }
    }
    fclose(in);
    fclose(out);
    return ok;
}

bool IsSamePhysicalFile(const char* path1, const char* path2) {
    wchar_t* w1 = Utf8ToWide(path1);
    wchar_t* w2 = Utf8ToWide(path2);
    if (!w1 || !w2) { free(w1); free(w2); return false; }

    wchar_t* p1 = PrepareWin32Path(w1);
    wchar_t* p2 = PrepareWin32Path(w2);
    free(w1);
    free(w2);

    HANDLE h1 = CreateFileW(p1 ? p1 : w1, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE h2 = CreateFileW(p2 ? p2 : w2, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(p1);
    free(p2);

    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) {
        if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
        if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
        return false;
    }

    BY_HANDLE_FILE_INFORMATION i1, i2;
    BOOL b1 = GetFileInformationByHandle(h1, &i1);
    BOOL b2 = GetFileInformationByHandle(h2, &i2);
    CloseHandle(h1);
    CloseHandle(h2);

    if (!b1 || !b2) return false;

    return (i1.dwVolumeSerialNumber == i2.dwVolumeSerialNumber &&
            i1.nFileIndexHigh == i2.nFileIndexHigh &&
            i1.nFileIndexLow == i2.nFileIndexLow);
}

void MakeDirectoryTree(const char* path) {
    char* buf = _strdup(path);
    if (!buf) return;
    NormalizeSlashes(buf);

    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
        buf[--len] = '\0';
    }

    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (!(strlen(buf) == 2 && isalpha((unsigned char)buf[0]) && buf[1] == ':')) {
                MakeDirUtf8(buf);
            }
            *p = '/';
        }
    }
    if (len > 0 && !(len == 2 && isalpha((unsigned char)buf[0]) && buf[1] == ':')) {
        MakeDirUtf8(buf);
    }
    free(buf);
}
