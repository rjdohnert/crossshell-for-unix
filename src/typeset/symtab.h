#ifndef SYMTAB_H
#define SYMTAB_H

#include "common.h"
#include "variable.h"

typedef struct SymbolTable SymbolTable;

typedef struct SymbolTableVTable {
    Variable*   (*Get)(const SymbolTable* self, const char* name);
    bool        (*Set)(SymbolTable* self, const char* name, const char* val,
                       unsigned int set_flags, unsigned int clr_flags, int width, int base);
    bool        (*Remove)(SymbolTable* self, const char* name);
    void        (*List)(const SymbolTable* self, unsigned int filter, bool match_all, FILE* out);
    bool        (*LoadState)(SymbolTable* self);
    bool        (*SaveState)(const SymbolTable* self);
    void        (*Destroy)(SymbolTable* self);
} SymbolTableVTable;

struct SymbolTable {
    const SymbolTableVTable* vptr;
    Variable** entries;
    size_t     count;
    size_t     capacity;
    wchar_t    state_dir[TS_PATH_CAP];
    wchar_t    state_file[TS_PATH_CAP];
};

SymbolTable* SymbolTable_Create(void);

#endif
