#include "jq_internal.h"

void JqParser_skip_whitespace(JqParser *self) {
    while (self->pos < self->len) {
        char c = self->src[self->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            self->pos++;
        } else {
            break;
        }
    }
}

char* JqParser_parse_raw_string(JqParser *self) {
    if (self->src[self->pos] != '"') return NULL;
    self->pos++; /* skip " */

    size_t cap = 32;
    size_t len = 0;
    char *buf = (char *)malloc(cap);

    while (self->pos < self->len) {
        char c = self->src[self->pos++];
        if (c == '"') {
            buf[len] = '\0';
            return buf;
        }
        if (c == '\\' && self->pos < self->len) {
            char esc = self->src[self->pos++];
            switch (esc) {
                case '"':  c = '"'; break;
                case '\\': c = '\\'; break;
                case '/':  c = '/'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    if (self->pos + 4 <= self->len) {
                        char hex[5] = {self->src[self->pos], self->src[self->pos+1],
                                       self->src[self->pos+2], self->src[self->pos+3], '\0'};
                        self->pos += 4;
                        unsigned int codepoint = (unsigned int)strtoul(hex, NULL, 16);
                        c = (codepoint < 128) ? (char)codepoint : '?';
                    }
                    break;
                }
                default: break;
            }
        }
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

JqValue* JqParser_parse_value_impl(JqParser *self) {
    JqParser_skip_whitespace(self);
    if (self->pos >= self->len) return NULL;

    char c = self->src[self->pos];

    if (c == 'n' && strncmp(&self->src[self->pos], "null", 4) == 0) {
        self->pos += 4;
        return JqValue_NewNull();
    }
    if (c == 't' && strncmp(&self->src[self->pos], "true", 4) == 0) {
        self->pos += 4;
        return JqValue_NewBool(true);
    }
    if (c == 'f' && strncmp(&self->src[self->pos], "false", 5) == 0) {
        self->pos += 5;
        return JqValue_NewBool(false);
    }
    if (c == '"') {
        char *str = JqParser_parse_raw_string(self);
        JqValue *v = JqValue_NewString(str);
        free(str);
        return v;
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        char *endptr;
        double num = strtod(&self->src[self->pos], &endptr);
        self->pos = (size_t)(endptr - self->src);
        return JqValue_NewNumber(num);
    }
    if (c == '[') {
        self->pos++;
        JqValue *arr = JqValue_NewArray();
        while (self->pos < self->len) {
            JqParser_skip_whitespace(self);
            if (self->pos < self->len && self->src[self->pos] == ']') {
                self->pos++;
                return arr;
            }
            JqValue *item = self->parse_value(self);
            if (!item) break;
            JqValue_ArrayAppend(arr, item);
            JqParser_skip_whitespace(self);
            if (self->pos < self->len && self->src[self->pos] == ',') {
                self->pos++;
            }
        }
        return arr;
    }
    if (c == '{') {
        self->pos++;
        JqValue *obj = JqValue_NewObject();
        while (self->pos < self->len) {
            JqParser_skip_whitespace(self);
            if (self->pos < self->len && self->src[self->pos] == '}') {
                self->pos++;
                return obj;
            }
            if (self->src[self->pos] != '"') break;
            char *key = JqParser_parse_raw_string(self);
            JqParser_skip_whitespace(self);
            if (self->pos < self->len && self->src[self->pos] == ':') {
                self->pos++;
            }
            JqValue *val = self->parse_value(self);
            if (key && val) {
                JqValue_ObjectSet(obj, key, val);
            }
            free(key);
            JqParser_skip_whitespace(self);
            if (self->pos < self->len && self->src[self->pos] == ',') {
                self->pos++;
            }
        }
        return obj;
    }

    return NULL;
}

JqParser *JqParser_New(const char *src) {
    JqParser *p = (JqParser *)calloc(1, sizeof(JqParser));
    p->src = src;
    p->len = strlen(src);
    p->parse_value = JqParser_parse_value_impl;
    return p;
}
