#include "jq_internal.h"

static void JqValue_destroy_impl(JqValue *self);
static JqValue *JqValue_clone_impl(const JqValue *self);
static bool JqValue_is_truthy_impl(const JqValue *self);

static const JqValueVTable g_JqValueVTable = {
    .destroy   = JqValue_destroy_impl,
    .clone     = JqValue_clone_impl,
    .serialize = JqValue_serialize_impl,
    .is_truthy = JqValue_is_truthy_impl
};

JqValue *JqValue_New(JqType t) {
    JqValue *v = (JqValue *)calloc(1, sizeof(JqValue));
    v->vptr = &g_JqValueVTable;
    v->type = t;
    return v;
}

JqValue *JqValue_NewNull(void) {
    return JqValue_New(JQ_NULL);
}

JqValue *JqValue_NewBool(bool b) {
    JqValue *v = JqValue_New(JQ_BOOL);
    v->as.bool_val = b;
    return v;
}

JqValue *JqValue_NewNumber(double n) {
    JqValue *v = JqValue_New(JQ_NUMBER);
    v->as.num_val = n;
    return v;
}

JqValue *JqValue_NewString(const char *s) {
    JqValue *v = JqValue_New(JQ_STRING);
    v->as.str_val = _strdup(s ? s : "");
    return v;
}

JqValue *JqValue_NewArray(void) {
    return JqValue_New(JQ_ARRAY);
}

JqValue *JqValue_NewObject(void) {
    return JqValue_New(JQ_OBJECT);
}

void JqValue_ArrayAppend(JqValue *self, JqValue *item) {
    if (self->type != JQ_ARRAY) return;
    if (self->as.arr.count >= self->as.arr.cap) {
        self->as.arr.cap = self->as.arr.cap ? self->as.arr.cap * 2 : 8;
        self->as.arr.items = (JqValue **)realloc(self->as.arr.items, sizeof(JqValue *) * self->as.arr.cap);
    }
    self->as.arr.items[self->as.arr.count++] = item;
}

void JqValue_ObjectSet(JqValue *self, const char *key, JqValue *val) {
    if (self->type != JQ_OBJECT) return;
    for (size_t i = 0; i < self->as.obj.count; i++) {
        if (strcmp(self->as.obj.entries[i].key, key) == 0) {
            self->as.obj.entries[i].val->vptr->destroy(self->as.obj.entries[i].val);
            self->as.obj.entries[i].val = val;
            return;
        }
    }
    if (self->as.obj.count >= self->as.obj.cap) {
        self->as.obj.cap = self->as.obj.cap ? self->as.obj.cap * 2 : 8;
        self->as.obj.entries = (JqObjectEntry *)realloc(self->as.obj.entries, sizeof(JqObjectEntry) * self->as.obj.cap);
    }
    self->as.obj.entries[self->as.obj.count].key = _strdup(key);
    self->as.obj.entries[self->as.obj.count].val = val;
    self->as.obj.count++;
}

JqValue *JqValue_ObjectGet(const JqValue *self, const char *key) {
    if (self->type != JQ_OBJECT) return NULL;
    for (size_t i = 0; i < self->as.obj.count; i++) {
        if (strcmp(self->as.obj.entries[i].key, key) == 0) {
            return self->as.obj.entries[i].val;
        }
    }
    return NULL;
}

void JqValue_destroy_impl(JqValue *self) {
    if (!self) return;
    if (self->type == JQ_STRING) {
        free(self->as.str_val);
    } else if (self->type == JQ_ARRAY) {
        for (size_t i = 0; i < self->as.arr.count; i++) {
            self->as.arr.items[i]->vptr->destroy(self->as.arr.items[i]);
        }
        free(self->as.arr.items);
    } else if (self->type == JQ_OBJECT) {
        for (size_t i = 0; i < self->as.obj.count; i++) {
            free(self->as.obj.entries[i].key);
            self->as.obj.entries[i].val->vptr->destroy(self->as.obj.entries[i].val);
        }
        free(self->as.obj.entries);
    }
    free(self);
}

JqValue* JqValue_clone_impl(const JqValue *self) {
    if (!self) return NULL;
    switch (self->type) {
        case JQ_NULL:   return JqValue_NewNull();
        case JQ_BOOL:   return JqValue_NewBool(self->as.bool_val);
        case JQ_NUMBER: return JqValue_NewNumber(self->as.num_val);
        case JQ_STRING: return JqValue_NewString(self->as.str_val);
        case JQ_ARRAY: {
            JqValue *dup = JqValue_NewArray();
            for (size_t i = 0; i < self->as.arr.count; i++) {
                JqValue_ArrayAppend(dup, self->as.arr.items[i]->vptr->clone(self->as.arr.items[i]));
            }
            return dup;
        }
        case JQ_OBJECT: {
            JqValue *dup = JqValue_NewObject();
            for (size_t i = 0; i < self->as.obj.count; i++) {
                JqValue_ObjectSet(dup, self->as.obj.entries[i].key,
                                  self->as.obj.entries[i].val->vptr->clone(self->as.obj.entries[i].val));
            }
            return dup;
        }
    }
    return JqValue_NewNull();
}

bool JqValue_is_truthy_impl(const JqValue *self) {
    if (!self || self->type == JQ_NULL) return false;
    if (self->type == JQ_BOOL) return self->as.bool_val;
    return true;
}
