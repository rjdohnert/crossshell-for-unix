#include "jq_internal.h"

void JqSerializer_write_indent(JqSerializer *self, int depth) {
    if (self->config->compact_output) return;
    fputc('\n', self->out);
    for (int i = 0; i < depth; i++) {
        if (self->config->tab_indent) {
            fputc('\t', self->out);
        } else {
            for (int s = 0; s < self->config->indent_spaces; s++) {
                fputc(' ', self->out);
            }
        }
    }
}

void JqSerializer_write_string(JqSerializer *self, const char *str, bool is_key) {
    if (self->enable_color) {
        fputs(is_key ? COLOR_KEY : COLOR_STRING, self->out);
    }
    fputc('"', self->out);
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\"': fputs("\\\"", self->out); break;
            case '\\': fputs("\\\\", self->out); break;
            case '\b': fputs("\\b", self->out); break;
            case '\f': fputs("\\f", self->out); break;
            case '\n': fputs("\\n", self->out); break;
            case '\r': fputs("\\r", self->out); break;
            case '\t': fputs("\\t", self->out); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    fprintf(self->out, "\\u%04x", (unsigned char)*p);
                } else if (self->config->ascii_output && (unsigned char)*p >= 0x80) {
                    fprintf(self->out, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, self->out);
                }
                break;
        }
    }
    fputc('"', self->out);
    if (self->enable_color) {
        fputs(COLOR_RESET, self->out);
    }
}

int compare_entries(const void *a, const void *b) {
    const JqObjectEntry *ea = (const JqObjectEntry *)a;
    const JqObjectEntry *eb = (const JqObjectEntry *)b;
    return strcmp(ea->key, eb->key);
}

void JqValue_serialize_impl(const JqValue *self, JqSerializer *ser, int depth) {
    if (!self) return;

    switch (self->type) {
        case JQ_NULL:
            if (ser->enable_color) fputs(COLOR_NULL, ser->out);
            fputs("null", ser->out);
            if (ser->enable_color) fputs(COLOR_RESET, ser->out);
            break;

        case JQ_BOOL:
            if (ser->enable_color) fputs(COLOR_BOOL, ser->out);
            fputs(self->as.bool_val ? "true" : "false", ser->out);
            if (ser->enable_color) fputs(COLOR_RESET, ser->out);
            break;

        case JQ_NUMBER:
            if (ser->enable_color) fputs(COLOR_NUMBER, ser->out);
            if (self->as.num_val == (int64_t)self->as.num_val) {
                fprintf(ser->out, "%lld", (long long)self->as.num_val);
            } else {
                fprintf(ser->out, "%g", self->as.num_val);
            }
            if (ser->enable_color) fputs(COLOR_RESET, ser->out);
            break;

        case JQ_STRING:
            ser->write_string(ser, self->as.str_val, false);
            break;

        case JQ_ARRAY:
            if (self->as.arr.count == 0) {
                fputs("[]", ser->out);
                return;
            }
            fputc('[', ser->out);
            for (size_t i = 0; i < self->as.arr.count; i++) {
                ser->write_indent(ser, depth + 1);
                self->as.arr.items[i]->vptr->serialize(self->as.arr.items[i], ser, depth + 1);
                if (i + 1 < self->as.arr.count) {
                    fputc(',', ser->out);
                    if (ser->config->compact_output) fputc(' ', ser->out);
                }
            }
            ser->write_indent(ser, depth);
            fputc(']', ser->out);
            break;

        case JQ_OBJECT:
            if (self->as.obj.count == 0) {
                fputs("{}", ser->out);
                return;
            }
            if (ser->config->sort_keys && self->as.obj.count > 1) {
                qsort(self->as.obj.entries, self->as.obj.count, sizeof(JqObjectEntry), compare_entries);
            }
            fputc('{', ser->out);
            for (size_t i = 0; i < self->as.obj.count; i++) {
                ser->write_indent(ser, depth + 1);
                ser->write_string(ser, self->as.obj.entries[i].key, true);
                fputs(ser->config->compact_output ? ":" : ": ", ser->out);
                self->as.obj.entries[i].val->vptr->serialize(self->as.obj.entries[i].val, ser, depth + 1);
                if (i + 1 < self->as.obj.count) {
                    fputc(',', ser->out);
                    if (ser->config->compact_output) fputc(' ', ser->out);
                }
            }
            ser->write_indent(ser, depth);
            fputc('}', ser->out);
            break;
    }
}

JqSerializer *JqSerializer_New(const JqConfig *cfg, FILE *out) {
    JqSerializer *ser = (JqSerializer *)calloc(1, sizeof(JqSerializer));
    ser->config = cfg;
    ser->out = out;
    ser->write_indent = JqSerializer_write_indent;
    ser->write_string = JqSerializer_write_string;

    bool is_term = _isatty(_fileno(out));
    if (cfg->color_output) {
        ser->enable_color = true;
    } else if (cfg->monochrome_output) {
        ser->enable_color = false;
    } else {
        ser->enable_color = is_term;
    }

    /* Enable Windows ANSI VT processing */
    if (ser->enable_color && is_term) {
        HANDLE hOut = (HANDLE)_get_osfhandle(_fileno(out));
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    return ser;
}
