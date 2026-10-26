#include "jq_internal.h"

static const JqFilterVTable g_PipelineVTable = { JqPipelineFilter_destroy, JqPipelineFilter_exec };

static const JqFilterVTable g_BuiltinVTable = { JqBuiltinFilter_destroy, JqBuiltinFilter_exec };

static const JqFilterVTable g_IteratorVTable = { JqIteratorFilter_destroy, JqIteratorFilter_exec };

static const JqFilterVTable g_IndexVTable = { JqIndexFilter_destroy, JqIndexFilter_exec };

static const JqFilterVTable g_KeyVTable = { JqKeyFilter_destroy, JqKeyFilter_exec };

static const JqFilterVTable g_IdentityVTable = { JqIdentityFilter_destroy, JqIdentityFilter_exec };

JqResultSet JqIdentityFilter_exec(JqFilter *self, const JqValue *input) {
    (void)self;
    JqResultSet rs = JqResultSet_Init();
    JqResultSet_Append(&rs, input->vptr->clone(input));
    return rs;
}

void JqIdentityFilter_destroy(JqFilter *self) { free(self); }

JqFilter *JqIdentityFilter_New(void) {
    JqIdentityFilter *f = (JqIdentityFilter *)malloc(sizeof(JqIdentityFilter));
    f->base.vptr = &g_IdentityVTable;
    return (JqFilter *)f;
}

JqResultSet JqKeyFilter_exec(JqFilter *self, const JqValue *input) {
    JqKeyFilter *kf = (JqKeyFilter *)self;
    JqResultSet rs = JqResultSet_Init();
    if (input->type == JQ_OBJECT) {
        JqValue *v = JqValue_ObjectGet(input, kf->key);
        if (v) {
            JqResultSet_Append(&rs, v->vptr->clone(v));
        } else {
            JqResultSet_Append(&rs, JqValue_NewNull());
        }
    } else {
        JqResultSet_Append(&rs, JqValue_NewNull());
    }
    return rs;
}

void JqKeyFilter_destroy(JqFilter *self) { free(self); }

JqFilter *JqKeyFilter_New(const char *key) {
    JqKeyFilter *f = (JqKeyFilter *)malloc(sizeof(JqKeyFilter));
    f->base.vptr = &g_KeyVTable;
    strncpy(f->key, key, 127);
    return (JqFilter *)f;
}

JqResultSet JqIndexFilter_exec(JqFilter *self, const JqValue *input) {
    JqIndexFilter *idx_f = (JqIndexFilter *)self;
    JqResultSet rs = JqResultSet_Init();
    if (input->type == JQ_ARRAY) {
        int idx = idx_f->index;
        if (idx < 0) idx += (int)input->as.arr.count;
        if (idx >= 0 && (size_t)idx < input->as.arr.count) {
            JqResultSet_Append(&rs, input->as.arr.items[idx]->vptr->clone(input->as.arr.items[idx]));
        } else {
            JqResultSet_Append(&rs, JqValue_NewNull());
        }
    } else {
        JqResultSet_Append(&rs, JqValue_NewNull());
    }
    return rs;
}

void JqIndexFilter_destroy(JqFilter *self) { free(self); }

JqFilter *JqIndexFilter_New(int index) {
    JqIndexFilter *f = (JqIndexFilter *)malloc(sizeof(JqIndexFilter));
    f->base.vptr = &g_IndexVTable;
    f->index = index;
    return (JqFilter *)f;
}

JqResultSet JqIteratorFilter_exec(JqFilter *self, const JqValue *input) {
    (void)self;
    JqResultSet rs = JqResultSet_Init();
    if (input->type == JQ_ARRAY) {
        for (size_t i = 0; i < input->as.arr.count; i++) {
            JqResultSet_Append(&rs, input->as.arr.items[i]->vptr->clone(input->as.arr.items[i]));
        }
    } else if (input->type == JQ_OBJECT) {
        for (size_t i = 0; i < input->as.obj.count; i++) {
            JqResultSet_Append(&rs, input->as.obj.entries[i].val->vptr->clone(input->as.obj.entries[i].val));
        }
    }
    return rs;
}

void JqIteratorFilter_destroy(JqFilter *self) { free(self); }

JqFilter *JqIteratorFilter_New(void) {
    JqIteratorFilter *f = (JqIteratorFilter *)malloc(sizeof(JqIteratorFilter));
    f->base.vptr = &g_IteratorVTable;
    return (JqFilter *)f;
}

JqResultSet JqBuiltinFilter_exec(JqFilter *self, const JqValue *input) {
    JqBuiltinFilter *bf = (JqBuiltinFilter *)self;
    JqResultSet rs = JqResultSet_Init();

    if (strcmp(bf->name, "keys") == 0) {
        JqValue *res = JqValue_NewArray();
        if (input->type == JQ_OBJECT) {
            for (size_t i = 0; i < input->as.obj.count; i++) {
                JqValue_ArrayAppend(res, JqValue_NewString(input->as.obj.entries[i].key));
            }
        }
        JqResultSet_Append(&rs, res);
    } else if (strcmp(bf->name, "length") == 0) {
        double len = 0;
        if (input->type == JQ_ARRAY)  len = (double)input->as.arr.count;
        if (input->type == JQ_OBJECT) len = (double)input->as.obj.count;
        if (input->type == JQ_STRING) len = (double)strlen(input->as.str_val);
        JqResultSet_Append(&rs, JqValue_NewNumber(len));
    } else if (strcmp(bf->name, "type") == 0) {
        const char *t = "null";
        if (input->type == JQ_BOOL)   t = "boolean";
        if (input->type == JQ_NUMBER) t = "number";
        if (input->type == JQ_STRING) t = "string";
        if (input->type == JQ_ARRAY)  t = "array";
        if (input->type == JQ_OBJECT) t = "object";
        JqResultSet_Append(&rs, JqValue_NewString(t));
    }
    return rs;
}

void JqBuiltinFilter_destroy(JqFilter *self) { free(self); }

JqFilter *JqBuiltinFilter_New(const char *name) {
    JqBuiltinFilter *f = (JqBuiltinFilter *)malloc(sizeof(JqBuiltinFilter));
    f->base.vptr = &g_BuiltinVTable;
    strncpy(f->name, name, 31);
    return (JqFilter *)f;
}

JqResultSet JqPipelineFilter_exec(JqFilter *self, const JqValue *input) {
    JqPipelineFilter *pf = (JqPipelineFilter *)self;
    JqResultSet intermediate = pf->left->vptr->execute(pf->left, input);
    JqResultSet final_res = JqResultSet_Init();

    for (size_t i = 0; i < intermediate.count; i++) {
        JqResultSet sub = pf->right->vptr->execute(pf->right, intermediate.items[i]);
        for (size_t j = 0; j < sub.count; j++) {
            JqResultSet_Append(&final_res, sub.items[j]);
        }
        free(sub.items); /* ownership moved to final_res */
    }
    JqResultSet_Free(&intermediate);
    return final_res;
}

void JqPipelineFilter_destroy(JqFilter *self) {
    JqPipelineFilter *pf = (JqPipelineFilter *)self;
    if (pf->left)  pf->left->vptr->destroy(pf->left);
    if (pf->right) pf->right->vptr->destroy(pf->right);
    free(pf);
}

JqFilter *JqPipelineFilter_New(JqFilter *l, JqFilter *r) {
    JqPipelineFilter *f = (JqPipelineFilter *)malloc(sizeof(JqPipelineFilter));
    f->base.vptr = &g_PipelineVTable;
    f->left = l;
    f->right = r;
    return (JqFilter *)f;
}

JqFilter* JqFilter_CompileSub(const char **expr) {
    while (**expr == ' ' || **expr == '\t') (*expr)++;

    if (**expr == '\0' || **expr == '|') return JqIdentityFilter_New();

    if (strncmp(*expr, "keys", 4) == 0 && !isalnum((unsigned char)(*expr)[4])) {
        *expr += 4;
        return JqBuiltinFilter_New("keys");
    }
    if (strncmp(*expr, "length", 6) == 0 && !isalnum((unsigned char)(*expr)[6])) {
        *expr += 6;
        return JqBuiltinFilter_New("length");
    }
    if (strncmp(*expr, "type", 4) == 0 && !isalnum((unsigned char)(*expr)[4])) {
        *expr += 4;
        return JqBuiltinFilter_New("type");
    }

    if (**expr == '.') {
        (*expr)++;
        if (**expr == '[') {
            (*expr)++;
            if (**expr == ']') {
                (*expr)++;
                return JqIteratorFilter_New();
            } else {
                int idx = atoi(*expr);
                while (**expr && **expr != ']') (*expr)++;
                if (**expr == ']') (*expr)++;
                return JqIndexFilter_New(idx);
            }
        }
        if (**expr == '\0' || **expr == '|' || **expr == ' ') {
            return JqIdentityFilter_New();
        }
        /* Identifier key access */
        char key_buf[128];
        int ki = 0;
        while (**expr && (isalnum((unsigned char)**expr) || **expr == '_' || **expr == '-')) {
            if (ki < 127) key_buf[ki++] = **expr;
            (*expr)++;
        }
        key_buf[ki] = '\0';
        return JqKeyFilter_New(key_buf);
    }

    return JqIdentityFilter_New();
}

JqFilter* JqFilter_Compile(const char *expr) {
    const char *p = expr;
    JqFilter *cur = JqFilter_CompileSub(&p);

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '|') {
            p++;
            JqFilter *rhs = JqFilter_CompileSub(&p);
            cur = JqPipelineFilter_New(cur, rhs);
        } else if (*p == '.') {
            /* Implicit chained access like .foo.bar */
            JqFilter *rhs = JqFilter_CompileSub(&p);
            cur = JqPipelineFilter_New(cur, rhs);
        } else {
            break;
        }
    }
    return cur;
}
