#include "symtab.h"
#include "strutil.h"

static Variable* ST_Get(const SymbolTable* self, const char* name) {
    for (size_t i = 0; i < self->count; ++i) {
        if (strcmp(self->entries[i]->name, name) == 0) {
            return self->entries[i];
        }
    }
    return NULL;
}

static bool ST_Set(SymbolTable* self, const char* name, const char* val,
                   unsigned int set_flags, unsigned int clr_flags, int width, int base) {
    Variable* existing = ST_Get(self, name);
    if (existing) {
        if (existing->flags & ATTR_READONLY) {
            fprintf(stderr, "typeset: %s: is read only\n", name);
            return false;
        }
        if (clr_flags) existing->vptr->ClearAttributes(existing, clr_flags);
        if (set_flags || width > 0 || base > 0) {
            existing->vptr->SetAttributes(existing, set_flags, width, base);
        }
        if (val) {
            existing->vptr->SetValue(existing, val);
        }
        return true;
    }

    if (self->count >= self->capacity) {
        size_t new_cap = self->capacity == 0 ? 32 : self->capacity * 2;
        Variable** new_entries = (Variable**)realloc(self->entries, new_cap * sizeof(Variable*));
        if (!new_entries) return false;
        self->entries = new_entries;
        self->capacity = new_cap;
    }

    Variable* v = Variable_Create(name, val ? val : "", set_flags, width, base);
    if (!v) return false;
    self->entries[self->count++] = v;
    return true;
}

static bool ST_Remove(SymbolTable* self, const char* name) {
    for (size_t i = 0; i < self->count; ++i) {
        if (strcmp(self->entries[i]->name, name) == 0) {
            Variable* v = self->entries[i];
            if (v->flags & ATTR_READONLY) {
                fprintf(stderr, "typeset: %s: is read only\n", name);
                return false;
            }
            if (v->flags & ATTR_EXPORT) {
                wchar_t* wname = Utf8ToUtf16(v->name);
                if (wname) SetEnvironmentVariableW(wname, NULL);
                free(wname);
            }
            v->vptr->Destroy(v);
            memmove(&self->entries[i], &self->entries[i + 1],
                    (self->count - i - 1) * sizeof(Variable*));
            self->count--;
            return true;
        }
    }
    fprintf(stderr, "typeset: %s: not found\n", name);
    return false;
}

static void ST_List(const SymbolTable* self, unsigned int filter, bool match_all, FILE* out) {
    for (size_t i = 0; i < self->count; ++i) {
        const Variable* v = self->entries[i];
        if (filter == 0) {
            v->vptr->PrintDefinition(v, out);
        } else {
            bool matches = match_all ? ((v->flags & filter) == filter) : ((v->flags & filter) != 0);
            if (matches) {
                v->vptr->PrintDefinition(v, out);
            }
        }
    }
}

static bool ST_LoadState(SymbolTable* self) {
    FILE* f = _wfopen(self->state_file, L"rb");
    if (!f) return false;

    bool ok = true;
    while (ok) {
        size_t nlen = 0, vlen = 0;
        unsigned int flags = 0;
        int width = 0, base = 0;

        if (fscanf(f, "%zu:", &nlen) != 1) break;
        if (nlen == 0 || nlen > MAX_IDENT_LEN) { ok = false; break; }

        char name[MAX_IDENT_LEN + 1];
        if (fread(name, 1, nlen, f) != nlen) { ok = false; break; }
        name[nlen] = '\0';

        if (fscanf(f, " %u %d %d %zu:", &flags, &width, &base, &vlen) != 4) { ok = false; break; }

        char* val = (char*)malloc(vlen + 1);
        if (!val) { ok = false; break; }
        if (fread(val, 1, vlen, f) != vlen) {
            free(val);
            ok = false;
            break;
        }
        val[vlen] = '\0';

        int c = fgetc(f);
        if (c != '\n' && c != EOF) ungetc(c, f);

        ST_Set(self, name, val, flags, 0, width, base);
        free(val);
    }

    fclose(f);
    if (!ok) {
        fprintf(stderr, "typeset: warning: state file corrupted, loaded partial data\n");
    }
    return true;
}

static bool ST_SaveState(const SymbolTable* self) {
    wchar_t temp_file[TS_PATH_CAP];
    swprintf(temp_file, TS_PATH_CAP, L"%s.tmp.%lu", self->state_file, GetCurrentProcessId());

    FILE* f = _wfopen(temp_file, L"wb");
    if (!f) return false;

    for (size_t i = 0; i < self->count; ++i) {
        const Variable* v = self->entries[i];
        size_t nlen = strlen(v->name);
        size_t vlen = strlen(v->value ? v->value : "");
        fprintf(f, "%zu:%s %u %d %d %zu:%s\n",
                nlen, v->name, v->flags, v->width, v->base, vlen, v->value ? v->value : "");
    }
    fclose(f);

    if (!MoveFileExW(temp_file, self->state_file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp_file);
        return false;
    }
    return true;
}

static void ST_Destroy(SymbolTable* self) {
    if (self) {
        for (size_t i = 0; i < self->count; ++i) {
            self->entries[i]->vptr->Destroy(self->entries[i]);
        }
        free(self->entries);
        free(self);
    }
}

static const SymbolTableVTable g_STVTable = {
    ST_Get,
    ST_Set,
    ST_Remove,
    ST_List,
    ST_LoadState,
    ST_SaveState,
    ST_Destroy
};

SymbolTable* SymbolTable_Create(void) {
    SymbolTable* st = (SymbolTable*)calloc(1, sizeof(SymbolTable));
    if (!st) return NULL;
    st->vptr = &g_STVTable;

    wchar_t local_app[TS_PATH_CAP];
    DWORD env_len = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app, TS_PATH_CAP);
    if (env_len > 0 && env_len < TS_PATH_CAP) {
        swprintf(st->state_dir, TS_PATH_CAP, L"%s\\typeset", local_app);
    } else {
        swprintf(st->state_dir, TS_PATH_CAP, L".\\typeset");
    }

    CreateDirectoryW(st->state_dir, NULL);
    swprintf(st->state_file, TS_PATH_CAP, L"%s\\session.db", st->state_dir);

    if (!st->vptr->LoadState(st)) {
        LPWCH env = GetEnvironmentStringsW();
        if (env) {
            LPWCH var = env;
            while (*var) {
                if (*var != L'=') {
                    wchar_t* eq = wcschr(var, L'=');
                    if (eq) {
                        *eq = L'\0';
                        char* name = Utf16ToUtf8(var);
                        char* val = Utf16ToUtf8(eq + 1);
                        *eq = L'=';

                        if (name && val && strlen(name) < MAX_IDENT_LEN) {
                            st->vptr->Set(st, name, val, ATTR_EXPORT, 0, 0, 10);
                        }
                        free(name);
                        free(val);
                    }
                }
                var += wcslen(var) + 1;
            }
            FreeEnvironmentStringsW(env);
        }
    }
    return st;
}
