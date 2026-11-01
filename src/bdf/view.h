#ifndef VIEW_H
#define VIEW_H

#include "common.h"
#include "volume.h"
#include "collection.h"

typedef struct ColumnWidths {
    size_t kb_w;
    size_t used_w;
    size_t avail_w;
    size_t iused_w;
    size_t ifree_w;
} ColumnWidths;

typedef struct IBdfView IBdfView;

typedef struct IBdfViewVTable {
    void (*RenderHeader)(const IBdfView* self, const ColumnWidths* cw, bool show_inodes);
    void (*RenderEntry)(const IBdfView* self, const IVolume* vol, const ColumnWidths* cw, bool show_inodes);
    void (*Destroy)(IBdfView* self);
} IBdfViewVTable;

struct IBdfView {
    const IBdfViewVTable* vptr;
};

IBdfView* HpuxBdfView_Create(void);
void CalculateColumnWidths(const VolumeCollection* volumes, ColumnWidths* cw);

#endif
