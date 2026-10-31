#include "variable.h"
#include "strutil.h"

static char* Var_FormatNewValue(const Variable* self, const char* raw) {
    char* val = SafeStrDup(raw);
    if (!val) return NULL;

    if (self->flags & ATTR_INTEGER) {
        int64_t num = 0;
        if (ParseIntegerBase(val, &num)) {
            char buffer[128];
            int b = (self->base >= 2 && self->base <= 36) ? self->base : 10;
            if (FormatIntegerBase(num, b, buffer, sizeof(buffer))) {
                free(val);
                val = SafeStrDup(buffer);
                if (!val) return NULL;
            }
        }
    }

    if (self->flags & ATTR_LOWER) {
        for (char* p = val; *p; ++p) *p = (char)tolower((unsigned char)*p);
    } else if (self->flags & ATTR_UPPER) {
        for (char* p = val; *p; ++p) *p = (char)toupper((unsigned char)*p);
    }

    if (self->width > 0) {
        size_t len = strlen(val);
        int w = self->width;
        char* formatted = (char*)malloc(w + 1);
        if (!formatted) return val;

        if (self->flags & ATTR_LJUST) {
            const char* start = val;
            while (*start && isspace((unsigned char)*start)) start++;
            size_t slen = strlen(start);
            if (slen >= (size_t)w) {
                memcpy(formatted, start, w);
                formatted[w] = '\0';
            } else {
                snprintf(formatted, w + 1, "%-*s", w, start);
            }
        } else if (self->flags & ATTR_ZFILL) {
            if (len >= (size_t)w) {
                memcpy(formatted, val + (len - w), w);
                formatted[w] = '\0';
            } else {
                int pad = w - (int)len;
                memset(formatted, '0', pad);
                memcpy(formatted + pad, val, len);
                formatted[w] = '\0';
            }
        } else if (self->flags & ATTR_RJUST) {
            if (len >= (size_t)w) {
                memcpy(formatted, val + (len - w), w);
                formatted[w] = '\0';
            } else {
                snprintf(formatted, w + 1, "%*s", w, val);
            }
        } else {
            free(formatted);
            return val;
        }

        free(val);
        val = formatted;
    }
    return val;
}

static bool Var_SetValue(Variable* self, const char* val) {
    if (self->flags & ATTR_READONLY) {
        fprintf(stderr, "typeset: %s: is read only\n", self->name);
        return false;
    }
    char* new_val = Var_FormatNewValue(self, val ? val : "");
    if (!new_val) return false;

    free(self->value);
    self->value = new_val;

    if (self->flags & ATTR_EXPORT) {
        wchar_t* wname = Utf8ToUtf16(self->name);
        wchar_t* wval = Utf8ToUtf16(self->value);
        if (wname && wval) SetEnvironmentVariableW(wname, wval);
        free(wname);
        free(wval);
    }
    return true;
}

static const char* Var_GetValue(const Variable* self) {
    return self->value ? self->value : "";
}

static void Var_SetAttributes(Variable* self, unsigned int flags, int width, int base) {
    self->flags |= flags;
    if (width > 0) self->width = width;
    if (base >= 2 && base <= 36) self->base = base;

    if (self->value) {
        char* reformatted = Var_FormatNewValue(self, self->value);
        if (reformatted) {
            free(self->value);
            self->value = reformatted;
        }
    }

    if (self->flags & ATTR_EXPORT) {
        wchar_t* wname = Utf8ToUtf16(self->name);
        wchar_t* wval = Utf8ToUtf16(self->value);
        if (wname && wval) SetEnvironmentVariableW(wname, wval);
        free(wname);
        free(wval);
    }
}

static void Var_ClearAttributes(Variable* self, unsigned int flags) {
    if (flags & ATTR_READONLY) {
        fprintf(stderr, "typeset: %s: cannot remove read-only attribute\n", self->name);
        return;
    }
    self->flags &= ~flags;

    if (flags & (ATTR_LJUST | ATTR_RJUST | ATTR_ZFILL)) {
        self->width = 0;
    }
    if (flags & ATTR_INTEGER) {
        self->base = 10;
    }

    if (flags & ATTR_EXPORT) {
        wchar_t* wname = Utf8ToUtf16(self->name);
        if (wname) SetEnvironmentVariableW(wname, NULL);
        free(wname);
    }
}

static void Var_PrintDefinition(const Variable* self, FILE* out) {
    fprintf(out, "typeset ");
    if (self->flags & ATTR_READONLY) fprintf(out, "-r ");
    if (self->flags & ATTR_EXPORT)   fprintf(out, "-x ");
    if (self->flags & ATTR_INTEGER) {
        if (self->base > 0 && self->base != 10) fprintf(out, "-i%d ", self->base);
        else fprintf(out, "-i ");
    }
    if (self->flags & ATTR_LOWER)    fprintf(out, "-l ");
    if (self->flags & ATTR_UPPER)    fprintf(out, "-u ");
    if (self->flags & ATTR_TAGGED)   fprintf(out, "-t ");
    if (self->flags & ATTR_ARRAY)    fprintf(out, "-a ");
    if (self->flags & ATTR_FUNCTION) fprintf(out, "-f ");
    if (self->flags & ATTR_LJUST)    fprintf(out, "-L%d ", self->width);
    if (self->flags & ATTR_RJUST)    fprintf(out, "-R%d ", self->width);
    if (self->flags & ATTR_ZFILL)    fprintf(out, "-Z%d ", self->width);

    fprintf(out, "%s=\"%s\"\n", self->name, self->value ? self->value : "");
}

static void Var_Destroy(Variable* self) {
    if (self) {
        free(self->name);
        free(self->value);
        free(self);
    }
}

static const VariableVTable g_VarVTable = {
    Var_SetValue,
    Var_GetValue,
    Var_SetAttributes,
    Var_ClearAttributes,
    Var_PrintDefinition,
    Var_Destroy
};

Variable* Variable_Create(const char* name, const char* value, unsigned int flags, int width, int base) {
    Variable* v = (Variable*)calloc(1, sizeof(Variable));
    if (!v) return NULL;
    v->vptr = &g_VarVTable;
    v->name = SafeStrDup(name);
    v->flags = flags;
    v->width = width;
    v->base = base;
    v->value = Var_FormatNewValue(v, value ? value : "");
    return v;
}
