#include "view.h"

typedef struct HpuxBdfView {
    IBdfView base;
} HpuxBdfView;

static void HpuxBdfView_RenderHeader(const IBdfView* self, const ColumnWidths* cw, bool show_inodes) {
    (void)self;
    if (show_inodes) {
        printf("%-20s %*s %*s %*s %5s %*s %*s %6s %s\n",
               "Filesystem",
               (int)cw->kb_w, "kbytes",
               (int)cw->used_w, "used",
               (int)cw->avail_w, "avail",
               "%used",
               (int)cw->iused_w, "iused",
               (int)cw->ifree_w, "ifree",
               "%iused",
               "Mounted on");
    } else {
        printf("%-20s %*s %*s %*s %5s %s\n",
               "Filesystem",
               (int)cw->kb_w, "kbytes",
               (int)cw->used_w, "used",
               (int)cw->avail_w, "avail",
               "%used",
               "Mounted on");
    }
}

static void HpuxBdfView_RenderEntry(const IBdfView* self, const IVolume* vol, const ColumnWidths* cw, bool show_inodes) {
    (void)self;
    const char* dev = vol->vptr->GetDeviceName(vol);
    const char* mnt = vol->vptr->GetMountPoint(vol);
    size_t devLen = strlen(dev);

    if (show_inodes) {
        if (devLen >= HPUX_NAME_MAX_COL) {
            printf("%s\n%-20s %*llu %*llu %*llu %4d%% %*llu %*llu %5d%% %s\n",
                   dev, "",
                   (int)cw->kb_w, (unsigned long long)vol->vptr->GetTotalKbytes(vol),
                   (int)cw->used_w, (unsigned long long)vol->vptr->GetUsedKbytes(vol),
                   (int)cw->avail_w, (unsigned long long)vol->vptr->GetAvailKbytes(vol),
                   vol->vptr->GetPercentUsed(vol),
                   (int)cw->iused_w, (unsigned long long)vol->vptr->GetUsedInodes(vol),
                   (int)cw->ifree_w, (unsigned long long)vol->vptr->GetFreeInodes(vol),
                   vol->vptr->GetPercentInodesUsed(vol),
                   mnt);
        } else {
            printf("%-20s %*llu %*llu %*llu %4d%% %*llu %*llu %5d%% %s\n",
                   dev,
                   (int)cw->kb_w, (unsigned long long)vol->vptr->GetTotalKbytes(vol),
                   (int)cw->used_w, (unsigned long long)vol->vptr->GetUsedKbytes(vol),
                   (int)cw->avail_w, (unsigned long long)vol->vptr->GetAvailKbytes(vol),
                   vol->vptr->GetPercentUsed(vol),
                   (int)cw->iused_w, (unsigned long long)vol->vptr->GetUsedInodes(vol),
                   (int)cw->ifree_w, (unsigned long long)vol->vptr->GetFreeInodes(vol),
                   vol->vptr->GetPercentInodesUsed(vol),
                   mnt);
        }
    } else {
        if (devLen >= HPUX_NAME_MAX_COL) {
            printf("%s\n%-20s %*llu %*llu %*llu %4d%% %s\n",
                   dev, "",
                   (int)cw->kb_w, (unsigned long long)vol->vptr->GetTotalKbytes(vol),
                   (int)cw->used_w, (unsigned long long)vol->vptr->GetUsedKbytes(vol),
                   (int)cw->avail_w, (unsigned long long)vol->vptr->GetAvailKbytes(vol),
                   vol->vptr->GetPercentUsed(vol),
                   mnt);
        } else {
            printf("%-20s %*llu %*llu %*llu %4d%% %s\n",
                   dev,
                   (int)cw->kb_w, (unsigned long long)vol->vptr->GetTotalKbytes(vol),
                   (int)cw->used_w, (unsigned long long)vol->vptr->GetUsedKbytes(vol),
                   (int)cw->avail_w, (unsigned long long)vol->vptr->GetAvailKbytes(vol),
                   vol->vptr->GetPercentUsed(vol),
                   mnt);
        }
    }
}

static void HpuxBdfView_Destroy(IBdfView* self) {
    if (self) free(self);
}

static const IBdfViewVTable g_HpuxBdfViewVTable = {
    HpuxBdfView_RenderHeader,
    HpuxBdfView_RenderEntry,
    HpuxBdfView_Destroy
};

IBdfView* HpuxBdfView_Create(void) {
    HpuxBdfView* v = (HpuxBdfView*)calloc(1, sizeof(HpuxBdfView));
    if (v) v->base.vptr = &g_HpuxBdfViewVTable;
    return (IBdfView*)v;
}

void CalculateColumnWidths(const VolumeCollection* volumes, ColumnWidths* cw) {
    cw->kb_w    = DEFAULT_NUM_WIDTH;
    cw->used_w  = DEFAULT_NUM_WIDTH;
    cw->avail_w = DEFAULT_NUM_WIDTH;
    cw->iused_w = DEFAULT_NUM_WIDTH;
    cw->ifree_w = DEFAULT_NUM_WIDTH;

    size_t count = volumes->vptr->Count(volumes);
    for (size_t i = 0; i < count; ++i) {
        IVolume* vol = volumes->vptr->GetAt(volumes, i);
        size_t d = CountDigits(vol->vptr->GetTotalKbytes(vol));
        if (d > cw->kb_w) cw->kb_w = d;

        d = CountDigits(vol->vptr->GetUsedKbytes(vol));
        if (d > cw->used_w) cw->used_w = d;

        d = CountDigits(vol->vptr->GetAvailKbytes(vol));
        if (d > cw->avail_w) cw->avail_w = d;

        d = CountDigits(vol->vptr->GetUsedInodes(vol));
        if (d > cw->iused_w) cw->iused_w = d;

        d = CountDigits(vol->vptr->GetFreeInodes(vol));
        if (d > cw->ifree_w) cw->ifree_w = d;
    }
}
