#ifndef CPIO_INODE_H
#define CPIO_INODE_H

#include "cpio.h"

// Hardlink tracking hash table (inode -> primary path).
typedef struct {
    InodeNode* buckets[INODE_TABLE_BUCKETS];
} InodeTable;

InodeTable* InodeTable_Create(void);
void        InodeTable_Add(InodeTable* t, uint32_t dev, uint32_t ino, const char* path);
const char* InodeTable_Find(InodeTable* t, uint32_t dev, uint32_t ino);
void        InodeTable_Free(InodeTable* t);

#endif // CPIO_INODE_H
