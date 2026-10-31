#ifndef VARIABLE_H
#define VARIABLE_H

#include "common.h"

typedef struct Variable Variable;

typedef struct VariableVTable {
    bool        (*SetValue)(Variable* self, const char* value);
    const char* (*GetValue)(const Variable* self);
    void        (*SetAttributes)(Variable* self, unsigned int flags, int width, int base);
    void        (*ClearAttributes)(Variable* self, unsigned int flags);
    void        (*PrintDefinition)(const Variable* self, FILE* out);
    void        (*Destroy)(Variable* self);
} VariableVTable;

struct Variable {
    const VariableVTable* vptr;
    char*        name;
    char*        value;
    unsigned int flags;
    int          width;
    int          base;
};

Variable* Variable_Create(const char* name, const char* value, unsigned int flags, int width, int base);

#endif
