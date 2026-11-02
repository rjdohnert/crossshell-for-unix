#include "cpio_inode.h"

/* ========================================================================= */
/* INODE & HARDLINK HASH TABLE                                               */
/* ========================================================================= */

InodeTable* InodeTable_Create(void) {
    return (InodeTable*)calloc(1, sizeof(InodeTable));
}

void InodeTable_Add(InodeTable* t, uint32_t dev, uint32_t ino, const char* path) {
    if (!t) return;
    uint32_t hash = ((dev * 31) ^ (ino * 17)) % INODE_TABLE_BUCKETS;
    InodeNode* node = (InodeNode*)malloc(sizeof(InodeNode));
    if (!node) return;
    node->dev = dev;
    node->ino = ino;
    node->primaryPath = _strdup(path);
    node->next = t->buckets[hash];
    t->buckets[hash] = node;
}

const char* InodeTable_Find(InodeTable* t, uint32_t dev, uint32_t ino) {
    if (!t) return NULL;
    uint32_t hash = ((dev * 31) ^ (ino * 17)) % INODE_TABLE_BUCKETS;
    InodeNode* cur = t->buckets[hash];
    while (cur) {
        if (cur->dev == dev && cur->ino == ino) {
            return cur->primaryPath;
        }
        cur = cur->next;
    }
    return NULL;
}

void InodeTable_Free(InodeTable* t) {
    if (!t) return;
    for (size_t i = 0; i < INODE_TABLE_BUCKETS; ++i) {
        InodeNode* cur = t->buckets[i];
        while (cur) {
            InodeNode* next = cur->next;
            free(cur->primaryPath);
            free(cur);
            cur = next;
        }
    }
    free(t);
}
