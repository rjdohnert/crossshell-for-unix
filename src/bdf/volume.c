#include "volume.h"

/* ---------------- DiskVolume ---------------- */

typedef struct DiskVolume {
    IVolume  base;
    char*    device_name;
    char*    mount_point;
    char*    fs_type;
    wchar_t* volume_guid;
    bool     is_local;
    uint64_t total_kb;
    uint64_t used_kb;
    uint64_t avail_kb;
    int      pct_used;
    uint64_t used_inodes;
    uint64_t free_inodes;
    int      pct_iused;
} DiskVolume;

#define DV(s) ((const DiskVolume*)(s))

static const char* DiskVolume_GetDeviceName(const IVolume* self) { return DV(self)->device_name; }
static const char* DiskVolume_GetMountPoint(const IVolume* self) { return DV(self)->mount_point; }
static const char* DiskVolume_GetFSType(const IVolume* self)     { return DV(self)->fs_type; }
static bool DiskVolume_IsLocal(const IVolume* self)              { return DV(self)->is_local; }
static uint64_t DiskVolume_GetTotalKbytes(const IVolume* self)   { return DV(self)->total_kb; }
static uint64_t DiskVolume_GetUsedKbytes(const IVolume* self)    { return DV(self)->used_kb; }
static uint64_t DiskVolume_GetAvailKbytes(const IVolume* self)   { return DV(self)->avail_kb; }
static int DiskVolume_GetPercentUsed(const IVolume* self)        { return DV(self)->pct_used; }
static uint64_t DiskVolume_GetUsedInodes(const IVolume* self)    { return DV(self)->used_inodes; }
static uint64_t DiskVolume_GetFreeInodes(const IVolume* self)    { return DV(self)->free_inodes; }
static int DiskVolume_GetPercentInodesUsed(const IVolume* self)  { return DV(self)->pct_iused; }

static bool DiskVolume_Sync(IVolume* self) {
    DiskVolume* dv = (DiskVolume*)self;
    WCHAR dasdPath[MAX_PATH];
    dasdPath[0] = L'\0';

    if (dv->volume_guid && dv->volume_guid[0]) {
        wcsncpy(dasdPath, dv->volume_guid, MAX_PATH - 1);
        dasdPath[MAX_PATH - 1] = L'\0';
    } else if (dv->mount_point && dv->mount_point[0] && dv->mount_point[1] == ':') {
        swprintf(dasdPath, MAX_PATH, L"\\\\.\\%c:", dv->mount_point[0]);
    }

    if (dasdPath[0] == L'\0') return false;

    size_t len = wcslen(dasdPath);
    if (len > 0 && dasdPath[len - 1] == L'\\') {
        dasdPath[len - 1] = L'\0';
    }

    HANDLE hVol = CreateFileW(LongPath(dasdPath), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hVol != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hVol);
        CloseHandle(hVol);
        return true;
    }
    return false;
}

static void DiskVolume_Destroy(IVolume* self) {
    if (self) {
        DiskVolume* dv = (DiskVolume*)self;
        free(dv->device_name);
        free(dv->mount_point);
        free(dv->fs_type);
        free(dv->volume_guid);
        free(dv);
    }
}

static const IVolumeVTable g_DiskVolumeVTable = {
    DiskVolume_GetDeviceName, DiskVolume_GetMountPoint, DiskVolume_GetFSType,
    DiskVolume_IsLocal, DiskVolume_GetTotalKbytes, DiskVolume_GetUsedKbytes,
    DiskVolume_GetAvailKbytes, DiskVolume_GetPercentUsed, DiskVolume_GetUsedInodes,
    DiskVolume_GetFreeInodes, DiskVolume_GetPercentInodesUsed,
    DiskVolume_Sync, DiskVolume_Destroy
};

IVolume* DiskVolume_Create(const char* device, const char* mount, const char* fstype,
                           const wchar_t* guid, bool local, uint64_t total, uint64_t used,
                           uint64_t avail, uint64_t iused, uint64_t ifree) {
    DiskVolume* dv = (DiskVolume*)calloc(1, sizeof(DiskVolume));
    if (!dv) return NULL;
    dv->base.vptr = &g_DiskVolumeVTable;
    dv->device_name = SafeStrDup(device ? device : "");
    dv->mount_point = SafeStrDup(mount ? mount : "");
    dv->fs_type = SafeStrDup(fstype ? fstype : "");
    if (guid) {
        dv->volume_guid = (wchar_t*)malloc((wcslen(guid) + 1) * sizeof(wchar_t));
        if (dv->volume_guid) wcscpy(dv->volume_guid, guid);
    }
    dv->is_local = local;
    dv->total_kb = total;
    dv->used_kb = used;
    dv->avail_kb = avail;

    uint64_t denom = used + avail;
    dv->pct_used = (denom > 0) ? (int)(((double)used / (double)denom * 100.0) + 0.5) : 0;

    dv->used_inodes = iused;
    dv->free_inodes = ifree;
    uint64_t idenom = iused + ifree;
    dv->pct_iused = (idenom > 0) ? (int)(((double)iused / (double)idenom * 100.0) + 0.5) : 0;
    return (IVolume*)dv;
}

/* ---------------- SwapVolume ---------------- */

typedef struct SwapVolume {
    IVolume  base;
    char*    device_name;
    char*    mount_point;
    char*    fs_type;
    uint64_t total_kb;
    uint64_t used_kb;
    uint64_t avail_kb;
    int      pct_used;
} SwapVolume;

#define SV(s) ((const SwapVolume*)(s))

static const char* SwapVolume_GetDeviceName(const IVolume* self) { return SV(self)->device_name; }
static const char* SwapVolume_GetMountPoint(const IVolume* self) { return SV(self)->mount_point; }
static const char* SwapVolume_GetFSType(const IVolume* self)     { return SV(self)->fs_type; }
static bool SwapVolume_IsLocal(const IVolume* self)              { (void)self; return true; }
static uint64_t SwapVolume_GetTotalKbytes(const IVolume* self)   { return SV(self)->total_kb; }
static uint64_t SwapVolume_GetUsedKbytes(const IVolume* self)    { return SV(self)->used_kb; }
static uint64_t SwapVolume_GetAvailKbytes(const IVolume* self)   { return SV(self)->avail_kb; }
static int SwapVolume_GetPercentUsed(const IVolume* self)        { return SV(self)->pct_used; }
static uint64_t SwapVolume_GetUsedInodes(const IVolume* self)    { (void)self; return 0; }
static uint64_t SwapVolume_GetFreeInodes(const IVolume* self)    { (void)self; return 0; }
static int SwapVolume_GetPercentInodesUsed(const IVolume* self)  { (void)self; return 0; }
static bool SwapVolume_Sync(IVolume* self)                       { (void)self; return true; }

static void SwapVolume_Destroy(IVolume* self) {
    if (self) {
        SwapVolume* sv = (SwapVolume*)self;
        free(sv->device_name);
        free(sv->mount_point);
        free(sv->fs_type);
        free(sv);
    }
}

static const IVolumeVTable g_SwapVolumeVTable = {
    SwapVolume_GetDeviceName, SwapVolume_GetMountPoint, SwapVolume_GetFSType,
    SwapVolume_IsLocal, SwapVolume_GetTotalKbytes, SwapVolume_GetUsedKbytes,
    SwapVolume_GetAvailKbytes, SwapVolume_GetPercentUsed, SwapVolume_GetUsedInodes,
    SwapVolume_GetFreeInodes, SwapVolume_GetPercentInodesUsed,
    SwapVolume_Sync, SwapVolume_Destroy
};

IVolume* SwapVolume_Create(const char* device, const char* mount,
                           uint64_t total, uint64_t used, uint64_t avail) {
    SwapVolume* sv = (SwapVolume*)calloc(1, sizeof(SwapVolume));
    if (!sv) return NULL;
    sv->base.vptr = &g_SwapVolumeVTable;
    sv->device_name = SafeStrDup(device ? device : "");
    sv->mount_point = SafeStrDup(mount ? mount : "");
    sv->fs_type = SafeStrDup("swap");
    sv->total_kb = total;
    sv->used_kb = used;
    sv->avail_kb = avail;

    uint64_t denom = used + avail;
    sv->pct_used = (denom > 0) ? (int)(((double)used / (double)denom * 100.0) + 0.5) : 0;
    return (IVolume*)sv;
}
