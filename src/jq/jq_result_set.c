#include "jq_internal.h"

JqResultSet JqResultSet_Init(void) {
    JqResultSet rs;
    rs.count = 0;
    rs.cap = 4;
    rs.items = (JqValue **)malloc(sizeof(JqValue *) * rs.cap);
    return rs;
}

void JqResultSet_Append(JqResultSet *rs, JqValue *v) {
    if (rs->count >= rs->cap) {
        rs->cap *= 2;
        rs->items = (JqValue **)realloc(rs->items, sizeof(JqValue *) * rs->cap);
    }
    rs->items[rs->count++] = v;
}

void JqResultSet_Free(JqResultSet *rs) {
    for (size_t i = 0; i < rs->count; i++) {
        rs->items[i]->vptr->destroy(rs->items[i]);
    }
    free(rs->items);
}
