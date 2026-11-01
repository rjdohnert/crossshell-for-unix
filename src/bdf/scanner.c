#include "scanner.h"

typedef struct Win32VolumeScanner {
    IVolumeScanner base;
    HMODULE hMpr;
    HMODULE hPsapi;
} Win32VolumeScanner;

typedef DWORD (WINAPI *PFN_WNetGetConnectionW)(LPCWSTR, LPWSTR, LPDWORD);
typedef struct _ENUM_PAGE_FILE_INFORMATION {
    DWORD cb;
    DWORD Reserved;
    SIZE_T TotalSize;
    SIZE_T TotalInUse;
    SIZE_T PeakUsage;
} ENUM_PAGE_FILE_INFORMATION, *PENUM_PAGE_FILE_INFORMATION;

typedef BOOL (CALLBACK *PENUM_PAGE_FILE_CALLBACKW)(LPVOID, PENUM_PAGE_FILE_INFORMATION, LPCWSTR);
typedef BOOL (WINAPI *PFN_EnumPageFilesW)(PENUM_PAGE_FILE_CALLBACKW, LPVOID);

static IVolume* Win32_ProbeVolume(const Win32VolumeScanner* self, const WCHAR* rootPath, bool local_only) {
    UINT driveType = GetDriveTypeW(rootPath);
    if (driveType == DRIVE_UNKNOWN || driveType == DRIVE_NO_ROOT_DIR) return NULL;

    bool is_local = (driveType != DRIVE_REMOTE);

    if (local_only && !is_local) {
        return NULL;
    }

    ULARGE_INTEGER free_caller, total_bytes, total_free;
    if (!GetDiskFreeSpaceExW(rootPath, &free_caller, &total_bytes, &total_free)) {
        return NULL;
    }

    WCHAR fsTypeBuf[MAX_PATH] = {0};
    GetVolumeInformationW(rootPath, NULL, 0, NULL, NULL, NULL, fsTypeBuf, MAX_PATH);

    WCHAR volumeGuid[MAX_PATH] = {0};
    GetVolumeNameForVolumeMountPointW(rootPath, volumeGuid, MAX_PATH);

    char devName[MAX_PATH] = {0};
    WCHAR uncPath[MAX_PATH] = {0};
    DWORD uncSize = MAX_PATH;

    if (driveType == DRIVE_REMOTE && self->hMpr) {
        PFN_WNetGetConnectionW pWNet = (PFN_WNetGetConnectionW)GetProcAddress(self->hMpr, "WNetGetConnectionW");
        if (pWNet && rootPath[0] && rootPath[1] == L':') {
            WCHAR driveLetterOnly[3] = { rootPath[0], L':', L'\0' };
            if (pWNet(driveLetterOnly, uncPath, &uncSize) == NO_ERROR) {
                char* u8 = Utf16ToUtf8(uncPath);
                if (u8) {
                    strncpy(devName, u8, sizeof(devName) - 1);
                    free(u8);
                }
            }
        }
    }

    if (devName[0] == '\0' && volumeGuid[0] != L'\0') {
        const WCHAR* start = volumeGuid;
        if (wcsncmp(start, L"\\\\?\\", 4) == 0) start += 4;
        WCHAR guidShort[MAX_PATH];
        wcsncpy(guidShort, start, MAX_PATH - 1);
        guidShort[MAX_PATH - 1] = L'\0';
        size_t slen = wcslen(guidShort);
        if (slen > 0 && guidShort[slen - 1] == L'\\') guidShort[slen - 1] = L'\0';

        WCHAR devPath[MAX_PATH] = {0};
        if (QueryDosDeviceW(guidShort, devPath, MAX_PATH) > 0) {
            char* u8 = Utf16ToUtf8(devPath);
            if (u8) {
                strncpy(devName, u8, sizeof(devName) - 1);
                free(u8);
            }
        }
    }

    if (devName[0] == '\0') {
        WCHAR dosDev[MAX_PATH] = {0};
        if (rootPath[0] && rootPath[1] == L':') {
            WCHAR driveLetterOnly[3] = { rootPath[0], L':', L'\0' };
            if (QueryDosDeviceW(driveLetterOnly, dosDev, MAX_PATH) > 0) {
                char* u8 = Utf16ToUtf8(dosDev);
                if (u8) {
                    strncpy(devName, u8, sizeof(devName) - 1);
                    free(u8);
                }
            }
        }
    }

    if (devName[0] == '\0') {
        char* u8 = Utf16ToUtf8(rootPath);
        if (u8) {
            strncpy(devName, u8, sizeof(devName) - 1);
            free(u8);
        }
    }

    char* mountPoint = Utf16ToUtf8(rootPath);
    char* fsType = Utf16ToUtf8(fsTypeBuf);

    uint64_t iused = 0, ifree = 0;
    if (volumeGuid[0] != L'\0') {
        WCHAR dasdPath[MAX_PATH];
        wcsncpy(dasdPath, volumeGuid, MAX_PATH - 1);
        dasdPath[MAX_PATH - 1] = L'\0';
        size_t glen = wcslen(dasdPath);
        if (glen > 0 && dasdPath[glen - 1] == L'\\') dasdPath[glen - 1] = L'\0';

        HANDLE hVol = CreateFileW(LongPath(dasdPath), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, 0, NULL);
        if (hVol != INVALID_HANDLE_VALUE) {
            NTFS_VOLUME_DATA_BUFFER ntfs;
            DWORD retBytes = 0;
            if (DeviceIoControl(hVol, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &ntfs, sizeof(ntfs), &retBytes, NULL)) {
                DWORD recSize = ntfs.BytesPerFileRecordSegment ? ntfs.BytesPerFileRecordSegment : 1024;
                DWORD clusSize = ntfs.BytesPerCluster ? ntfs.BytesPerCluster : 4096;
                iused = (uint64_t)(ntfs.MftValidDataLength.QuadPart / recSize);
                ifree = (uint64_t)((ntfs.FreeClusters.QuadPart * clusSize) / recSize);
            }
            CloseHandle(hVol);
        }
    }

    if (iused == 0 && ifree == 0) {
        DWORD secPerClus = 0, bytesPerSec = 0, freeClus = 0, totalClus = 0;
        if (GetDiskFreeSpaceW(rootPath, &secPerClus, &bytesPerSec, &freeClus, &totalClus)) {
            iused = (uint64_t)(totalClus - freeClus);
            ifree = (uint64_t)freeClus;
        }
    }

    uint64_t total_kb = total_bytes.QuadPart / 1024;
    uint64_t free_kb = total_free.QuadPart / 1024;
    uint64_t avail_kb = free_caller.QuadPart / 1024;
    uint64_t used_kb = (total_kb >= free_kb) ? (total_kb - free_kb) : 0;

    IVolume* vol = DiskVolume_Create(devName, mountPoint ? mountPoint : "", fsType ? fsType : "",
                                     volumeGuid, is_local, total_kb, used_kb, avail_kb, iused, ifree);
    free(mountPoint);
    free(fsType);
    if (!vol) {
        return NULL;
    }
    return vol;
}

static VolumeCollection* Win32_ScanAllVolumes(IVolumeScanner* base, bool local_only, const char* fstype) {
    Win32VolumeScanner* self = (Win32VolumeScanner*)base;
    VolumeCollection* col = VolumeCollection_Create();

    WCHAR driveStrings[1024];
    DWORD len = GetLogicalDriveStringsW(1024, driveStrings);
    if (len > 0 && len <= 1024) {
        const WCHAR* p = driveStrings;
        while (*p) {
            IVolume* vol = Win32_ProbeVolume(self, p, local_only);
            if (vol) {
                bool accept = true;
                if (fstype && _stricmp(vol->vptr->GetFSType(vol), fstype) != 0) accept = false;

                if (accept && !col->vptr->ContainsMount(col, vol->vptr->GetMountPoint(vol))) {
                    col->vptr->Add(col, vol);
                } else {
                    vol->vptr->Destroy(vol);
                }
            }
            p += wcslen(p) + 1;
        }
    }

    WCHAR volName[MAX_PATH];
    HANDLE hFind = FindFirstVolumeW(volName, MAX_PATH);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DWORD pathLen = 0;
            GetVolumePathNamesForVolumeNameW(volName, NULL, 0, &pathLen);
            if (GetLastError() == ERROR_MORE_DATA && pathLen > 0) {
                WCHAR* pathNames = (WCHAR*)malloc(pathLen * sizeof(WCHAR));
                if (pathNames) {
                    if (GetVolumePathNamesForVolumeNameW(volName, pathNames, pathLen, &pathLen)) {
                        WCHAR* mountPath = pathNames;
                        while (*mountPath) {
                            char* u8Mount = Utf16ToUtf8(mountPath);
                            if (u8Mount) {
                                if (!col->vptr->ContainsMount(col, u8Mount)) {
                                    IVolume* vol = Win32_ProbeVolume(self, mountPath, local_only);
                                    if (vol) {
                                        bool accept = true;
                                        if (fstype && _stricmp(vol->vptr->GetFSType(vol), fstype) != 0) accept = false;

                                        if (accept) col->vptr->Add(col, vol);
                                        else vol->vptr->Destroy(vol);
                                    }
                                }
                                free(u8Mount);
                            }
                            mountPath += wcslen(mountPath) + 1;
                        }
                    }
                    free(pathNames);
                }
            }
        } while (FindNextVolumeW(hFind, volName, MAX_PATH));
        FindVolumeClose(hFind);
    }

    return col;
}

static VolumeCollection* Win32_ScanPathVolumes(IVolumeScanner* base, int path_count, char** paths,
                                               bool local_only, const char* fstype, int* out_failed) {
    Win32VolumeScanner* self = (Win32VolumeScanner*)base;
    VolumeCollection* col = VolumeCollection_Create();
    int failed = 0;

    for (int i = 0; i < path_count; ++i) {
        wchar_t* wPath = Utf8ToUtf16(paths[i]);
        if (!wPath) {
            failed++;
            continue;
        }

        DWORD reqLen = GetFullPathNameW(wPath, 0, NULL, NULL);
        if (reqLen == 0) {
            fprintf(stderr, "bdf: %s: No such file or directory\n", paths[i]);
            free(wPath);
            failed++;
            continue;
        }

        WCHAR* fullPath = (WCHAR*)malloc(reqLen * sizeof(WCHAR));
        if (!fullPath || GetFullPathNameW(wPath, reqLen, fullPath, NULL) == 0 ||
            GetFileAttributesW(fullPath) == INVALID_FILE_ATTRIBUTES) {
            fprintf(stderr, "bdf: %s: No such file or directory\n", paths[i]);
            free(wPath);
            free(fullPath);
            failed++;
            continue;
        }
        free(wPath);

        WCHAR rootPath[MAX_PATH];
        if (!GetVolumePathNameW(fullPath, rootPath, MAX_PATH)) {
            fprintf(stderr, "bdf: %s: cannot determine file system\n", paths[i]);
            free(fullPath);
            failed++;
            continue;
        }
        free(fullPath);

        IVolume* vol = Win32_ProbeVolume(self, rootPath, local_only);
        if (!vol) {
            fprintf(stderr, "bdf: %s: unable to query file system\n", paths[i]);
            failed++;
            continue;
        }

        bool accept = true;
        if (fstype && _stricmp(vol->vptr->GetFSType(vol), fstype) != 0) accept = false;

        if (accept) col->vptr->Add(col, vol);
        else vol->vptr->Destroy(vol);
    }

    if (out_failed) *out_failed = failed;
    return col;
}

typedef struct SwapEnumContext {
    VolumeCollection* col;
    const char* fstype;
} SwapEnumContext;

static BOOL CALLBACK SwapEnumCallback(LPVOID pContext, PENUM_PAGE_FILE_INFORMATION pInfo, LPCWSTR lpFileName) {
    SwapEnumContext* ctx = (SwapEnumContext*)pContext;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD pageSize = si.dwPageSize ? si.dwPageSize : 4096;

    uint64_t total_kb = ((uint64_t)pInfo->TotalSize * pageSize) / 1024;
    uint64_t used_kb  = ((uint64_t)pInfo->TotalInUse * pageSize) / 1024;
    uint64_t avail_kb = (total_kb >= used_kb) ? (total_kb - used_kb) : 0;

    char* devName = Utf16ToUtf8(lpFileName);
    IVolume* swapVol = SwapVolume_Create(devName ? devName : "pagefile.sys", "[swap]",
                                         total_kb, used_kb, avail_kb);
    free(devName);

    if (ctx->fstype && _stricmp(swapVol->vptr->GetFSType(swapVol), ctx->fstype) != 0) {
        swapVol->vptr->Destroy(swapVol);
        return TRUE;
    }

    ctx->col->vptr->Add(ctx->col, swapVol);
    return TRUE;
}

static VolumeCollection* Win32_ScanSwapVolumes(IVolumeScanner* base, const char* fstype) {
    Win32VolumeScanner* self = (Win32VolumeScanner*)base;
    VolumeCollection* col = VolumeCollection_Create();
    bool enumerated = false;

    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");
    PFN_EnumPageFilesW pEnum = NULL;
    if (hK32) {
        pEnum = (PFN_EnumPageFilesW)GetProcAddress(hK32, "EnumPageFilesW");
    }
    if (!pEnum && self->hPsapi) {
        pEnum = (PFN_EnumPageFilesW)GetProcAddress(self->hPsapi, "EnumPageFilesW");
    }

    if (pEnum) {
        SwapEnumContext ctx = { col, fstype };
        enumerated = pEnum(SwapEnumCallback, &ctx) && (col->vptr->Count(col) > 0);
    }

    if (!enumerated) {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms) && ms.ullTotalPageFile > 0) {
            uint64_t total_kb = ms.ullTotalPageFile / 1024;
            uint64_t avail_kb = ms.ullAvailPageFile / 1024;
            uint64_t used_kb = (total_kb >= avail_kb) ? (total_kb - avail_kb) : 0;
            IVolume* swapVol = SwapVolume_Create("\\Device\\Pagefile", "[swap]", total_kb, used_kb, avail_kb);
            if (fstype && _stricmp(swapVol->vptr->GetFSType(swapVol), fstype) != 0) {
                swapVol->vptr->Destroy(swapVol);
            } else {
                col->vptr->Add(col, swapVol);
            }
        }
    }
    return col;
}

static void Win32_DestroyScanner(IVolumeScanner* base) {
    if (base) {
        Win32VolumeScanner* self = (Win32VolumeScanner*)base;
        if (self->hMpr) FreeLibrary(self->hMpr);
        if (self->hPsapi) FreeLibrary(self->hPsapi);
        free(self);
    }
}

static const IVolumeScannerVTable g_Win32VolumeScannerVTable = {
    Win32_ScanAllVolumes,
    Win32_ScanPathVolumes,
    Win32_ScanSwapVolumes,
    Win32_DestroyScanner
};

IVolumeScanner* Win32VolumeScanner_Create(void) {
    Win32VolumeScanner* scan = (Win32VolumeScanner*)calloc(1, sizeof(Win32VolumeScanner));
    if (!scan) return NULL;
    scan->base.vptr = &g_Win32VolumeScannerVTable;
    scan->hMpr = LoadLibraryExW(L"mpr.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    scan->hPsapi = LoadLibraryExW(L"psapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return (IVolumeScanner*)scan;
}
