/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

#define DIFF3_VERSION L"diff3 3.10"
#define HASH_SEED 0xcbf29ce484222325ULL
#define HASH_PRIME 0x100000001b3ULL

/* ========================================================================= */
/* CONFIGURATION & ENUMS                                                     */
/* ========================================================================= */

typedef enum {
    OUT_DIFF3_DEFAULT, // Authentic ====, ====1, ====2, ====3
    OUT_DIFF3_MERGE,   // -m: 3-way merged file with conflict brackets
    OUT_DIFF3_ED_E,    // -e: ed script incorporating file2->file3 into file1
    OUT_DIFF3_ED_3,    // -3: ed script with non-overlapping changes only
    OUT_DIFF3_ED_X,    // -x: ed script with overlapping changes only
    OUT_DIFF3_ED_E_BRACKET // -E, -X: ed script with bracketed conflicts
} OutputMode;

typedef enum {
    COLOR_NEVER,
    COLOR_AUTO,
    COLOR_ALWAYS
} ColorMode;

typedef struct {
    OutputMode mode;
    bool show_all;             // -A: show full base in conflicts
    bool treat_as_text;        // -a: force text comparison
    bool initial_tab;          // -T
    bool strip_trailing_cr;    // --strip-trailing-cr
    bool append_wq_to_ed;      // -i
    ColorMode color_mode;      // --color
    bool use_color;            // Resolved dynamically
    wchar_t* labels[3];        // -L labels for File 1, 2, 3
} Diff3Config;

/* ========================================================================= */
/* OOP INTERFACE: IFileSource                                                */
/* ========================================================================= */

typedef struct IFileSource IFileSource;
typedef struct IFileSourceVtbl {
    void (*Destroy)(IFileSource* self);
    size_t (*GetLineCount)(const IFileSource* self);
    const char* (*GetLine)(const IFileSource* self, size_t index, size_t* out_len);
    uint64_t (*GetLineHash)(const IFileSource* self, size_t index);
    bool (*IsBinary)(const IFileSource* self);
    const wchar_t* (*GetPath)(const IFileSource* self);
    const wchar_t* (*GetLabel)(const IFileSource* self);
} IFileSourceVtbl;

struct IFileSource {
    const IFileSourceVtbl* vptr;
};

/* --- Implementation: MemoryMappedFileSource --- */
typedef struct {
    IFileSource base;
    wchar_t* path;
    wchar_t* label;
    HANDLE file_handle;
    HANDLE map_handle;
    const char* data;
    uint64_t file_size;
    size_t* line_offsets;
    size_t* line_lengths;
    uint64_t* line_hashes;
    size_t line_count;
    bool is_binary;
} MemoryMappedFileSource;

static uint64_t HashLine(const char* str, size_t len) {
    uint64_t hash = HASH_SEED;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (unsigned char)str[i];
        hash *= HASH_PRIME;
    }
    return hash;
}

static void MMapSource_Destroy(IFileSource* base_self) {
    if (!base_self) return;
    MemoryMappedFileSource* self = (MemoryMappedFileSource*)base_self;
    if (self->data) UnmapViewOfFile(self->data);
    if (self->map_handle) CloseHandle(self->map_handle);
    if (self->file_handle != INVALID_HANDLE_VALUE) CloseHandle(self->file_handle);
    free(self->line_offsets);
    free(self->line_lengths);
    free(self->line_hashes);
    free(self->path);
    free(self->label);
    free(self);
}

static size_t MMapSource_GetLineCount(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->line_count;
}

static const char* MMapSource_GetLine(const IFileSource* base_self, size_t index, size_t* out_len) {
    const MemoryMappedFileSource* self = (const MemoryMappedFileSource*)base_self;
    if (index >= self->line_count) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    if (out_len) *out_len = self->line_lengths[index];
    return self->data + self->line_offsets[index];
}

static uint64_t MMapSource_GetLineHash(const IFileSource* base_self, size_t index) {
    const MemoryMappedFileSource* self = (const MemoryMappedFileSource*)base_self;
    return (index < self->line_count) ? self->line_hashes[index] : 0;
}

static bool MMapSource_IsBinary(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->is_binary;
}

static const wchar_t* MMapSource_GetPath(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->path;
}

static const wchar_t* MMapSource_GetLabel(const IFileSource* base_self) {
    const MemoryMappedFileSource* self = (const MemoryMappedFileSource*)base_self;
    return self->label ? self->label : self->path;
}

static const IFileSourceVtbl g_MMapSourceVtbl = {
    MMapSource_Destroy,
    MMapSource_GetLineCount,
    MMapSource_GetLine,
    MMapSource_GetLineHash,
    MMapSource_IsBinary,
    MMapSource_GetPath,
    MMapSource_GetLabel
};

IFileSource* MemoryMappedFileSource_Create(const wchar_t* path, const wchar_t* label, bool strip_cr) {
    MemoryMappedFileSource* self = (MemoryMappedFileSource*)calloc(1, sizeof(MemoryMappedFileSource));
    if (!self) return NULL;
    self->base.vptr = &g_MMapSourceVtbl;
    self->path = _wcsdup(path);
    if (label) self->label = _wcsdup(label);

    self->file_handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (self->file_handle == INVALID_HANDLE_VALUE) {
        MMapSource_Destroy((IFileSource*)self);
        return NULL;
    }

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(self->file_handle, &sz)) {
        MMapSource_Destroy((IFileSource*)self);
        return NULL;
    }
    self->file_size = (uint64_t)sz.QuadPart;
    if (self->file_size == 0) return (IFileSource*)self;

    self->map_handle = CreateFileMappingW(self->file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!self->map_handle) {
        MMapSource_Destroy((IFileSource*)self);
        return NULL;
    }

    self->data = (const char*)MapViewOfFile(self->map_handle, FILE_MAP_READ, 0, 0, 0);
    if (!self->data) {
        MMapSource_Destroy((IFileSource*)self);
        return NULL;
    }

    size_t capacity = 512;
    self->line_offsets = (size_t*)malloc(capacity * sizeof(size_t));
    self->line_lengths = (size_t*)malloc(capacity * sizeof(size_t));
    self->line_hashes  = (uint64_t*)malloc(capacity * sizeof(uint64_t));

    size_t start = 0;
    for (size_t i = 0; i < self->file_size; ++i) {
        if (self->data[i] == '\0') self->is_binary = true;

        if (self->data[i] == '\n' || i == self->file_size - 1) {
            size_t end = (self->data[i] == '\n') ? i : i + 1;
            size_t len = end - start;

            if (strip_cr && len > 0 && self->data[start + len - 1] == '\r') {
                len--;
            }

            if (self->line_count >= capacity) {
                if (capacity > SIZE_MAX / (2 * sizeof(size_t))) {
                    MMapSource_Destroy((IFileSource*)self);
                    return NULL;
                }
                capacity *= 2;
                self->line_offsets = (size_t*)realloc(self->line_offsets, capacity * sizeof(size_t));
                self->line_lengths = (size_t*)realloc(self->line_lengths, capacity * sizeof(size_t));
                self->line_hashes  = (uint64_t*)realloc(self->line_hashes, capacity * sizeof(uint64_t));
            }

            self->line_offsets[self->line_count] = start;
            self->line_lengths[self->line_count] = len;
            self->line_hashes[self->line_count]  = HashLine(self->data + start, len);
            self->line_count++;
            start = i + 1;
        }
    }
    return (IFileSource*)self;
}

/* ========================================================================= */
/* PAIRWISE DIFF ENGINE (MYERS ALGORITHM)                                    */
/* ========================================================================= */

typedef struct {
    size_t start1, len1; // Base/Old File
    size_t start2, len2; // New File
} Hunk2;

typedef struct {
    Hunk2* hunks;
    size_t count;
    size_t capacity;
} Diff2HunkList;

typedef struct IDiff2Engine IDiff2Engine;
typedef struct IDiff2EngineVtbl {
    void (*Destroy)(IDiff2Engine* self);
    Diff2HunkList* (*ComputeDiff)(IDiff2Engine* self, const IFileSource* f1, const IFileSource* f2);
} IDiff2EngineVtbl;

struct IDiff2Engine {
    const IDiff2EngineVtbl* vptr;
};

typedef struct {
    IDiff2Engine base;
} MyersDiff2Engine;

static bool LinesAreEqual(const IFileSource* f1, size_t idx1, const IFileSource* f2, size_t idx2) {
    if (f1->vptr->GetLineHash(f1, idx1) != f2->vptr->GetLineHash(f2, idx2)) return false;
    size_t l1, l2;
    const char* s1 = f1->vptr->GetLine(f1, idx1, &l1);
    const char* s2 = f2->vptr->GetLine(f2, idx2, &l2);
    if (l1 != l2) return false;
    return memcmp(s1, s2, l1) == 0;
}

static void Myers2_Destroy(IDiff2Engine* self) { free(self); }

typedef enum {
    OP_MATCH,
    OP_INSERT,
    OP_DELETE
} EditOpType;

typedef struct {
    EditOpType type;
    size_t line1;
    size_t line2;
} EditOp;

static Diff2HunkList* Myers2_ComputeDiff(IDiff2Engine* self, const IFileSource* f1, const IFileSource* f2) {
    (void)self;
    size_t n = f1->vptr->GetLineCount(f1);
    size_t m = f2->vptr->GetLineCount(f2);
    size_t max_d = n + m;
    intptr_t offset = (intptr_t)max_d;

    if (max_d > (SIZE_MAX / 2) / sizeof(intptr_t)) return NULL;

    intptr_t* v = (intptr_t*)calloc(2 * max_d + 1, sizeof(intptr_t));
    if (!v) return NULL;

    typedef struct { intptr_t* snap; } Trace;
    Trace* trace = (Trace*)calloc(max_d + 1, sizeof(Trace));
    if (!trace) { free(v); return NULL; }

    v[offset + 1] = 0;
    size_t actual_d = 0;

    for (size_t d = 0; d <= max_d; ++d) {
        actual_d = d;
        trace[d].snap = (intptr_t*)malloc((2 * d + 3) * sizeof(intptr_t));
        if (!trace[d].snap) goto cleanup_fail;
        memcpy(trace[d].snap, &v[offset - (intptr_t)d - 1], (2 * d + 3) * sizeof(intptr_t));

        for (intptr_t k = -(intptr_t)d; k <= (intptr_t)d; k += 2) {
            intptr_t x;
            if (k == -(intptr_t)d || (k != (intptr_t)d && v[offset + k - 1] < v[offset + k + 1])) {
                x = v[offset + k + 1];
            } else {
                x = v[offset + k - 1] + 1;
            }
            intptr_t y = x - k;

            while (x < (intptr_t)n && y < (intptr_t)m && LinesAreEqual(f1, (size_t)x, f2, (size_t)y)) {
                x++; y++;
            }
            v[offset + k] = x;
            if (x >= (intptr_t)n && y >= (intptr_t)m) goto backtrack;
        }
    }

backtrack: {
        size_t script_cap = actual_d + n + m + 16;
        EditOp* ops = (EditOp*)malloc(script_cap * sizeof(EditOp));
        if (!ops) goto cleanup_fail;
        size_t op_count = 0;

        intptr_t cx = (intptr_t)n, cy = (intptr_t)m;
        for (intptr_t d = (intptr_t)actual_d; d > 0; --d) {
            intptr_t* v_prev = trace[d].snap;
            intptr_t k = cx - cy;
            intptr_t prev_k = 0;
            if (k == -d || (k != d && v_prev[(k - 1) + d + 1] < v_prev[(k + 1) + d + 1])) {
                prev_k = k + 1;
            } else {
                prev_k = k - 1;
            }
            intptr_t px = v_prev[prev_k + d + 1];
            intptr_t py = px - prev_k;

            while (cx > px && cy > py) {
                cx--; cy--;
                ops[op_count++] = (EditOp){ OP_MATCH, (size_t)cx, (size_t)cy };
            }
            if (cx == px) {
                cy--;
                ops[op_count++] = (EditOp){ OP_INSERT, (size_t)cx, (size_t)cy };
            } else {
                cx--;
                ops[op_count++] = (EditOp){ OP_DELETE, (size_t)cx, (size_t)cy };
            }
        }
        while (cx > 0 && cy > 0) {
            cx--; cy--;
            ops[op_count++] = (EditOp){ OP_MATCH, (size_t)cx, (size_t)cy };
        }

        /* Convert reversed ops (N,M down to 0,0) into forward Diff2HunkList */
        Diff2HunkList* list = (Diff2HunkList*)calloc(1, sizeof(Diff2HunkList));
        list->capacity = 32;
        list->hunks = (Hunk2*)malloc(list->capacity * sizeof(Hunk2));

        bool in_hunk = false;
        Hunk2 cur_hunk = {0};

        for (size_t idx = op_count; idx > 0; --idx) {
            EditOp op = ops[idx - 1];
            if (op.type == OP_MATCH) {
                if (in_hunk) {
                    if (list->count >= list->capacity) {
                        list->capacity *= 2;
                        list->hunks = (Hunk2*)realloc(list->hunks, list->capacity * sizeof(Hunk2));
                    }
                    list->hunks[list->count++] = cur_hunk;
                    in_hunk = false;
                }
            } else if (op.type == OP_DELETE) {
                if (!in_hunk) {
                    in_hunk = true;
                    cur_hunk = (Hunk2){ .start1 = op.line1, .len1 = 1, .start2 = op.line2, .len2 = 0 };
                } else {
                    cur_hunk.len1++;
                }
            } else if (op.type == OP_INSERT) {
                if (!in_hunk) {
                    in_hunk = true;
                    cur_hunk = (Hunk2){ .start1 = op.line1, .len1 = 0, .start2 = op.line2, .len2 = 1 };
                } else {
                    cur_hunk.len2++;
                }
            }
        }

        if (in_hunk) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                list->hunks = (Hunk2*)realloc(list->hunks, list->capacity * sizeof(Hunk2));
            }
            list->hunks[list->count++] = cur_hunk;
        }

        free(ops);
        for (size_t d = 0; d <= actual_d; ++d) free(trace[d].snap);
        free(trace);
        free(v);
        return list;
    }

cleanup_fail:
    for (size_t d = 0; d <= actual_d; ++d) {
        if (trace[d].snap) free(trace[d].snap);
    }
    free(trace);
    free(v);
    return NULL;
}

static const IDiff2EngineVtbl g_MyersDiff2Vtbl = { Myers2_Destroy, Myers2_ComputeDiff };

IDiff2Engine* MyersDiff2Engine_Create(void) {
    MyersDiff2Engine* self = (MyersDiff2Engine*)malloc(sizeof(MyersDiff2Engine));
    if (!self) return NULL;
    self->base.vptr = &g_MyersDiff2Vtbl;
    return (IDiff2Engine*)self;
}

/* ========================================================================= */
/* 3-WAY RECONCILIATION ENGINE                                               */
/* ========================================================================= */

typedef enum {
    DIFF3_SYNC,       // All three files identical in this range
    DIFF3_DIFF_1,     // Only File 1 changed (File 2 == File 3)
    DIFF3_DIFF_2,     // Files 1 and 3 changed identically (File 2 differs)
    DIFF3_DIFF_3,     // Only File 3 changed (File 1 == File 2)
    DIFF3_DIFF_ALL    // Conflict: Both File 1 and File 3 changed differently
} Diff3BlockType;

typedef struct {
    Diff3BlockType type;
    size_t s1, l1; // Range in File 1 (Mine)
    size_t s2, l2; // Range in File 2 (Old/Base)
    size_t s3, l3; // Range in File 3 (Yours)
} Diff3Block;

typedef struct {
    Diff3Block* blocks;
    size_t count;
    size_t capacity;
} Diff3Script;

typedef struct IDiff3Engine IDiff3Engine;
typedef struct IDiff3EngineVtbl {
    void (*Destroy)(IDiff3Engine* self);
    Diff3Script* (*Reconcile)(IDiff3Engine* self, const IFileSource* f1, const IFileSource* f2, const IFileSource* f3);
} IDiff3EngineVtbl;

struct IDiff3Engine {
    const IDiff3EngineVtbl* vptr;
};

typedef struct {
    IDiff3Engine base;
    IDiff2Engine* diff2;
} StandardDiff3Engine;

static void Diff3Engine_Destroy(IDiff3Engine* base_self) {
    if (!base_self) return;
    StandardDiff3Engine* self = (StandardDiff3Engine*)base_self;
    if (self->diff2) self->diff2->vptr->Destroy(self->diff2);
    free(self);
}

static bool CheckRangesMatch(const IFileSource* fA, size_t sA, size_t lA,
                             const IFileSource* fB, size_t sB, size_t lB) {
    if (lA != lB) return false;
    for (size_t i = 0; i < lA; ++i) {
        if (!LinesAreEqual(fA, sA + i, fB, sB + i)) return false;
    }
    return true;
}

static Diff3Script* Diff3Engine_Reconcile(IDiff3Engine* base_self,
                                         const IFileSource* f1,
                                         const IFileSource* f2,
                                         const IFileSource* f3) {
    StandardDiff3Engine* self = (StandardDiff3Engine*)base_self;
    Diff2HunkList* h1 = self->diff2->vptr->ComputeDiff(self->diff2, f2, f1); // Base -> Mine
    Diff2HunkList* h3 = self->diff2->vptr->ComputeDiff(self->diff2, f2, f3); // Base -> Yours

    if (!h1 || !h3) {
        if (h1) { free(h1->hunks); free(h1); }
        if (h3) { free(h3->hunks); free(h3); }
        return NULL;
    }

    Diff3Script* script = (Diff3Script*)calloc(1, sizeof(Diff3Script));
    script->capacity = 64;
    script->blocks = (Diff3Block*)malloc(script->capacity * sizeof(Diff3Block));

    size_t i1 = 0, i3 = 0;
    size_t b2 = 0; // Current pointer in Base (File 2)
    size_t b1 = 0; // Current pointer in File 1
    size_t b3 = 0; // Current pointer in File 3

    while (i1 < h1->count || i3 < h3->count) {
        bool has1 = (i1 < h1->count);
        bool has3 = (i3 < h3->count);

        size_t next_b2 = (size_t)-1;
        if (has1 && h1->hunks[i1].start1 < next_b2) next_b2 = h1->hunks[i1].start1;
        if (has3 && h3->hunks[i3].start1 < next_b2) next_b2 = h3->hunks[i3].start1;

        // Emit SYNC block if base advanced with no edits
        if (next_b2 > b2) {
            size_t sync_len = next_b2 - b2;
            if (script->count >= script->capacity) {
                script->capacity *= 2;
                script->blocks = (Diff3Block*)realloc(script->blocks, script->capacity * sizeof(Diff3Block));
            }
            script->blocks[script->count++] = (Diff3Block){
                DIFF3_SYNC, b1, sync_len, b2, sync_len, b3, sync_len
            };
            b1 += sync_len;
            b2 += sync_len;
            b3 += sync_len;
        }

        // Determine extent of conflict / hunk clustering
        size_t cluster_b2_start = next_b2;
        size_t cluster_b2_end = cluster_b2_start;

        size_t k1 = i1, k3 = i3;
        if (k1 < h1->count && h1->hunks[k1].start1 == cluster_b2_start) {
            if (h1->hunks[k1].start1 + h1->hunks[k1].len1 > cluster_b2_end)
                cluster_b2_end = h1->hunks[k1].start1 + h1->hunks[k1].len1;
            k1++;
        }
        if (k3 < h3->count && h3->hunks[k3].start1 == cluster_b2_start) {
            if (h3->hunks[k3].start1 + h3->hunks[k3].len1 > cluster_b2_end)
                cluster_b2_end = h3->hunks[k3].start1 + h3->hunks[k3].len1;
            k3++;
        }

        bool expanded = true;
        while (expanded) {
            expanded = false;
            if (k1 < h1->count && h1->hunks[k1].start1 <= cluster_b2_end) {
                if (h1->hunks[k1].start1 + h1->hunks[k1].len1 > cluster_b2_end) {
                    cluster_b2_end = h1->hunks[k1].start1 + h1->hunks[k1].len1;
                }
                k1++;
                expanded = true;
            }
            if (k3 < h3->count && h3->hunks[k3].start1 <= cluster_b2_end) {
                if (h3->hunks[k3].start1 + h3->hunks[k3].len1 > cluster_b2_end) {
                    cluster_b2_end = h3->hunks[k3].start1 + h3->hunks[k3].len1;
                }
                k3++;
                expanded = true;
            }
        }

        size_t f2_len = cluster_b2_end - cluster_b2_start;
        size_t f1_len = f2_len;
        size_t f3_len = f2_len;

        bool in1 = (k1 > i1);
        bool in3 = (k3 > i3);

        if (in1) {
            size_t sum_del = 0, sum_ins = 0;
            for (size_t idx = i1; idx < k1; ++idx) {
                sum_del += h1->hunks[idx].len1;
                sum_ins += h1->hunks[idx].len2;
            }
            f1_len = f2_len - sum_del + sum_ins;
        }

        if (in3) {
            size_t sum_del = 0, sum_ins = 0;
            for (size_t idx = i3; idx < k3; ++idx) {
                sum_del += h3->hunks[idx].len1;
                sum_ins += h3->hunks[idx].len2;
            }
            f3_len = f2_len - sum_del + sum_ins;
        }

        // Classify Block
        Diff3BlockType type;
        if (in1 && !in3) {
            type = DIFF3_DIFF_1;
        } else if (!in1 && in3) {
            type = DIFF3_DIFF_3;
        } else {
            // Both changed: check if they changed identically
            if (CheckRangesMatch(f1, b1, f1_len, f3, b3, f3_len)) {
                type = DIFF3_DIFF_2; // Identical change
            } else {
                type = DIFF3_DIFF_ALL; // Genuine conflict
            }
        }

        if (script->count >= script->capacity) {
            script->capacity *= 2;
            script->blocks = (Diff3Block*)realloc(script->blocks, script->capacity * sizeof(Diff3Block));
        }

        script->blocks[script->count++] = (Diff3Block){
            type, b1, f1_len, cluster_b2_start, f2_len, b3, f3_len
        };

        b1 += f1_len;
        b3 += f3_len;
        b2 = cluster_b2_end;
        i1 = k1;
        i3 = k3;
    }

    // Trailing sync lines up to end of base
    size_t n2 = f2->vptr->GetLineCount(f2);
    if (b2 < n2) {
        size_t rem = n2 - b2;
        if (script->count >= script->capacity) {
            script->capacity += 2;
            script->blocks = (Diff3Block*)realloc(script->blocks, script->capacity * sizeof(Diff3Block));
        }
        script->blocks[script->count++] = (Diff3Block){
            DIFF3_SYNC, b1, rem, b2, rem, b3, rem
        };
    }

    free(h1->hunks); free(h1);
    free(h3->hunks); free(h3);
    return script;
}

static const IDiff3EngineVtbl g_StandardDiff3EngineVtbl = {
    Diff3Engine_Destroy,
    Diff3Engine_Reconcile
};

IDiff3Engine* StandardDiff3Engine_Create(void) {
    StandardDiff3Engine* self = (StandardDiff3Engine*)malloc(sizeof(StandardDiff3Engine));
    if (!self) return NULL;
    self->base.vptr = &g_StandardDiff3EngineVtbl;
    self->diff2 = MyersDiff2Engine_Create();
    return (IDiff3Engine*)self;
}

/* ========================================================================= */
/* OOP INTERFACE: IOutputFormatter                                           */
/* ========================================================================= */

typedef struct IOutputFormatter IOutputFormatter;
typedef struct IOutputFormatterVtbl {
    void (*Destroy)(IOutputFormatter* self);
    bool (*Format)(IOutputFormatter* self, const IFileSource* f1, const IFileSource* f2,
                   const IFileSource* f3, const Diff3Script* script);
} IOutputFormatterVtbl;

struct IOutputFormatter {
    const IOutputFormatterVtbl* vptr;
};

#define ANSI_RED     "\x1b[31;1m"
#define ANSI_GREEN   "\x1b[32;1m"
#define ANSI_BLUE    "\x1b[34;1m"
#define ANSI_YELLOW  "\x1b[33;1m"
#define ANSI_RESET   "\x1b[0m"

static void SafePrintLines(const IFileSource* f, size_t start, size_t len, const char* prefix) {
    for (size_t i = 0; i < len; ++i) {
        size_t l;
        const char* str = f->vptr->GetLine(f, start + i, &l);
        if (prefix) printf("%s", prefix);
        fwrite(str, 1, l, stdout);
        putchar('\n');
    }
}

/* --- Concrete Implementation: DefaultDiff3Formatter (Authentic diff3) --- */
typedef struct {
    IOutputFormatter base;
    Diff3Config config;
} DefaultDiff3Formatter;

static void DefaultFmt_Destroy(IOutputFormatter* self) { free(self); }

static void PrintHunkInfo(int file_num, size_t start, size_t len) {
    if (len == 0) printf("%da\n", (int)start);
    else if (len == 1) printf("%dc\n", (int)(start + 1));
    else printf("%d,%dc\n", (int)(start + 1), (int)(start + len));
}

static bool DefaultFmt_Format(IOutputFormatter* base_self, const IFileSource* f1,
                             const IFileSource* f2, const IFileSource* f3,
                             const Diff3Script* script) {
    DefaultDiff3Formatter* self = (DefaultDiff3Formatter*)base_self;
    const char* pfx = self->config.initial_tab ? "\t" : "  ";

    for (size_t i = 0; i < script->count; ++i) {
        const Diff3Block* b = &script->blocks[i];
        if (b->type == DIFF3_SYNC) continue;

        if (b->type == DIFF3_DIFF_ALL) printf("====\n");
        else if (b->type == DIFF3_DIFF_1) printf("====1\n");
        else if (b->type == DIFF3_DIFF_2) printf("====2\n");
        else if (b->type == DIFF3_DIFF_3) printf("====3\n");

        printf("1:"); PrintHunkInfo(1, b->s1, b->l1);
        if (b->type != DIFF3_DIFF_3 && b->l1 > 0) SafePrintLines(f1, b->s1, b->l1, pfx);

        printf("2:"); PrintHunkInfo(2, b->s2, b->l2);
        if (b->type != DIFF3_DIFF_2 && b->l2 > 0) SafePrintLines(f2, b->s2, b->l2, pfx);

        printf("3:"); PrintHunkInfo(3, b->s3, b->l3);
        if (b->type != DIFF3_DIFF_1 && b->l3 > 0) SafePrintLines(f3, b->s3, b->l3, pfx);
    }
    return false; // Default format never counts as conflict exit
}

static const IOutputFormatterVtbl g_DefaultFmtVtbl = { DefaultFmt_Destroy, DefaultFmt_Format };

/* --- Concrete Implementation: MergeDiff3Formatter (-m, -A, -E) --- */
typedef struct {
    IOutputFormatter base;
    Diff3Config config;
} MergeDiff3Formatter;

static void MergeFmt_Destroy(IOutputFormatter* self) { free(self); }

static bool MergeFmt_Format(IOutputFormatter* base_self, const IFileSource* f1,
                           const IFileSource* f2, const IFileSource* f3,
                           const Diff3Script* script) {
    MergeDiff3Formatter* self = (MergeDiff3Formatter*)base_self;
    bool has_conflict = false;

    for (size_t i = 0; i < script->count; ++i) {
        const Diff3Block* b = &script->blocks[i];

        switch (b->type) {
            case DIFF3_SYNC:
                SafePrintLines(f1, b->s1, b->l1, NULL);
                break;
            case DIFF3_DIFF_1: // Mine changed cleanly
                SafePrintLines(f1, b->s1, b->l1, NULL);
                break;
            case DIFF3_DIFF_2: // Both changed identically
                SafePrintLines(f1, b->s1, b->l1, NULL);
                break;
            case DIFF3_DIFF_3: // Yours changed cleanly
                SafePrintLines(f3, b->s3, b->l3, NULL);
                break;
            case DIFF3_DIFF_ALL: // Overlapping conflict!
                has_conflict = true;
                if (self->config.use_color) printf(ANSI_RED);
                printf("<<<<<<< %ls\n", f1->vptr->GetLabel(f1));
                if (self->config.use_color) printf(ANSI_RESET);

                SafePrintLines(f1, b->s1, b->l1, NULL);

                if (self->config.show_all) {
                    if (self->config.use_color) printf(ANSI_YELLOW);
                    printf("||||||| %ls\n", f2->vptr->GetLabel(f2));
                    if (self->config.use_color) printf(ANSI_RESET);
                    SafePrintLines(f2, b->s2, b->l2, NULL);
                }

                if (self->config.use_color) printf(ANSI_BLUE);
                printf("=======\n");
                if (self->config.use_color) printf(ANSI_RESET);

                SafePrintLines(f3, b->s3, b->l3, NULL);

                if (self->config.use_color) printf(ANSI_RED);
                printf(">>>>>>> %ls\n", f3->vptr->GetLabel(f3));
                if (self->config.use_color) printf(ANSI_RESET);
                break;
        }
    }
    return has_conflict;
}

static const IOutputFormatterVtbl g_MergeFmtVtbl = { MergeFmt_Destroy, MergeFmt_Format };

/* --- Concrete Implementation: EdScriptDiff3Formatter (-e, -x, -3, -E) --- */
typedef struct {
    IOutputFormatter base;
    Diff3Config config;
} EdScriptDiff3Formatter;

static void EdFmt_Destroy(IOutputFormatter* self) { free(self); }

static bool EdFmt_Format(IOutputFormatter* base_self, const IFileSource* f1,
                         const IFileSource* f2, const IFileSource* f3,
                         const Diff3Script* script) {
    EdScriptDiff3Formatter* self = (EdScriptDiff3Formatter*)base_self;
    bool has_conflict = false;

    // Ed scripts run backwards so line numbers remain valid during edits
    for (size_t idx = script->count; idx > 0; --idx) {
        const Diff3Block* b = &script->blocks[idx - 1];

        bool emit = false;
        bool bracket = false;

        if (b->type == DIFF3_DIFF_3) {
            if (self->config.mode == OUT_DIFF3_ED_E || self->config.mode == OUT_DIFF3_ED_3 ||
                self->config.mode == OUT_DIFF3_ED_E_BRACKET) {
                emit = true;
            }
        } else if (b->type == DIFF3_DIFF_ALL) {
            has_conflict = true;
            if (self->config.mode == OUT_DIFF3_ED_E || self->config.mode == OUT_DIFF3_ED_X ||
                self->config.mode == OUT_DIFF3_ED_E_BRACKET) {
                emit = true;
                bracket = (self->config.mode == OUT_DIFF3_ED_E_BRACKET);
            }
        }

        if (emit) {
            size_t start = b->s1 + 1;
            size_t end = b->s1 + b->l1;

            if (b->l1 == 0) printf("%zua\n", b->s1);
            else if (b->l1 == 1) printf("%zuc\n", start);
            else printf("%zu,%zuc\n", start, end);

            if (bracket) {
                printf("<<<<<<< %ls\n", f1->vptr->GetLabel(f1));
                SafePrintLines(f1, b->s1, b->l1, NULL);
                printf("=======\n");
            }

            SafePrintLines(f3, b->s3, b->l3, NULL);

            if (bracket) {
                printf(">>>>>>> %ls\n", f3->vptr->GetLabel(f3));
            }
            printf(".\n");
        }
    }

    if (self->config.append_wq_to_ed) {
        printf("w\nq\n");
    }
    return has_conflict;
}

static const IOutputFormatterVtbl g_EdFmtVtbl = { EdFmt_Destroy, EdFmt_Format };

/* Formatter Factory */
IOutputFormatter* FormatterFactory_Create(const Diff3Config* cfg) {
    if (cfg->mode == OUT_DIFF3_MERGE) {
        MergeDiff3Formatter* m = (MergeDiff3Formatter*)malloc(sizeof(MergeDiff3Formatter));
        if (m) { m->base.vptr = &g_MergeFmtVtbl; m->config = *cfg; }
        return (IOutputFormatter*)m;
    } else if (cfg->mode == OUT_DIFF3_DEFAULT) {
        DefaultDiff3Formatter* d = (DefaultDiff3Formatter*)malloc(sizeof(DefaultDiff3Formatter));
        if (d) { d->base.vptr = &g_DefaultFmtVtbl; d->config = *cfg; }
        return (IOutputFormatter*)d;
    } else {
        EdScriptDiff3Formatter* e = (EdScriptDiff3Formatter*)malloc(sizeof(EdScriptDiff3Formatter));
        if (e) { e->base.vptr = &g_EdFmtVtbl; e->config = *cfg; }
        return (IOutputFormatter*)e;
    }
}

/* ========================================================================= */
/* CONSOLE MANAGER                                                           */
/* ========================================================================= */

typedef struct {
    HANDLE hOut;
    bool is_console;
} ConsoleManager;

static void ConsoleManager_Init(ConsoleManager* self) {
    self->hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    self->is_console = (GetFileType(self->hOut) == FILE_TYPE_CHAR) && GetConsoleMode(self->hOut, &mode);
    if (self->is_console) {
        SetConsoleMode(self->hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

/* ========================================================================= */
/* HELP & VERSION                                                            */
/* ========================================================================= */

static void DisplayHelp(void) {
    printf("Usage: diff3 [OPTION]... MYFILE OLDFILE YOURFILE\n");
    printf("Compare three files line by line.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -A, --show-all              output all changes, bracketing conflicts\n");
    printf("  -e, --ed                    output ed script incorporating changes\n");
    printf("                                from OLDFILE to YOURFILE into MYFILE\n");
    printf("  -E, --show-overlap          like -e, but bracket conflicts\n");
    printf("  -3, --easy-only             like -e, but incorporate only nonoverlapping changes\n");
    printf("  -x, --overlap-only          like -e, but incorporate only overlapping changes\n");
    printf("  -X                          like -x, but bracket conflicts\n");
    printf("  -m, --merge                 output actual merged file, according to\n");
    printf("                                -A if no other options are given\n");
    printf("  -a, --text                  treat all files as text\n");
    printf("  -i                          append 'w' and 'q' commands to ed scripts\n");
    printf("  -T, --initial-tab           make tabs line up by prepending a tab\n");
    printf("  -L, --label=LABEL           use LABEL instead of file name (can be repeated 3 times)\n");
    printf("      --strip-trailing-cr     strip trailing carriage return on input\n");
    printf("      --color[=WHEN]          colorize output; WHEN can be 'never', 'always',\n");
    printf("                                or 'auto' (default)\n");
    printf("      --help                  display this help and exit\n");
    printf("  -v, --version               output version information and exit\n\n");
    printf("Exit status is 0 if successful, 1 if conflicts were found, 2 if trouble.\n");
}

static void DisplayVersion(void) {
    printf("%ls\n", DIFF3_VERSION);
    printf("Self-contained 3-way Myers Reconciliation Engine\n");
}

/* ========================================================================= */
/* MAIN CONTROLLER                                                           */
/* ========================================================================= */

int wmain(int argc, wchar_t* argv[]) {
    Diff3Config config = {
        .mode = OUT_DIFF3_DEFAULT,
        .show_all = false,
        .treat_as_text = false,
        .initial_tab = false,
        .strip_trailing_cr = false,
        .append_wq_to_ed = false,
        .color_mode = COLOR_AUTO,
        .use_color = false,
        .labels = { NULL, NULL, NULL }
    };

    wchar_t* files[3] = { NULL, NULL, NULL };
    int file_idx = 0;
    int label_idx = 0;

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];
        if (wcscmp(arg, L"--help") == 0) { DisplayHelp(); return 0; }
        if (wcscmp(arg, L"-v") == 0 || wcscmp(arg, L"--version") == 0) { DisplayVersion(); return 0; }
        if (wcscmp(arg, L"-m") == 0 || wcscmp(arg, L"--merge") == 0) { config.mode = OUT_DIFF3_MERGE; }
        else if (wcscmp(arg, L"-A") == 0 || wcscmp(arg, L"--show-all") == 0) { config.mode = OUT_DIFF3_MERGE; config.show_all = true; }
        else if (wcscmp(arg, L"-e") == 0 || wcscmp(arg, L"--ed") == 0) { config.mode = OUT_DIFF3_ED_E; }
        else if (wcscmp(arg, L"-E") == 0 || wcscmp(arg, L"--show-overlap") == 0) { config.mode = OUT_DIFF3_ED_E_BRACKET; }
        else if (wcscmp(arg, L"-3") == 0 || wcscmp(arg, L"--easy-only") == 0) { config.mode = OUT_DIFF3_ED_3; }
        else if (wcscmp(arg, L"-x") == 0 || wcscmp(arg, L"--overlap-only") == 0) { config.mode = OUT_DIFF3_ED_X; }
        else if (wcscmp(arg, L"-X") == 0) { config.mode = OUT_DIFF3_ED_E_BRACKET; }
        else if (wcscmp(arg, L"-a") == 0 || wcscmp(arg, L"--text") == 0) { config.treat_as_text = true; }
        else if (wcscmp(arg, L"-i") == 0) { config.append_wq_to_ed = true; }
        else if (wcscmp(arg, L"-T") == 0 || wcscmp(arg, L"--initial-tab") == 0) { config.initial_tab = true; }
        else if (wcscmp(arg, L"--strip-trailing-cr") == 0) { config.strip_trailing_cr = true; }
        else if (wcsncmp(arg, L"--color", 7) == 0) {
            if (arg[7] == L'=') {
                if (wcscmp(&arg[8], L"always") == 0) config.color_mode = COLOR_ALWAYS;
                else if (wcscmp(&arg[8], L"never") == 0) config.color_mode = COLOR_NEVER;
                else config.color_mode = COLOR_AUTO;
            } else config.color_mode = COLOR_ALWAYS;
        }
        else if (wcscmp(arg, L"-L") == 0 || wcsncmp(arg, L"--label=", 8) == 0) {
            wchar_t* lbl = (wcscmp(arg, L"-L") == 0) ? ((i + 1 < argc) ? argv[++i] : NULL) : &arg[8];
            if (label_idx < 3) config.labels[label_idx++] = lbl;
        }
        else if (arg[0] == L'-' && arg[1] == L'L') {
            if (label_idx < 3) config.labels[label_idx++] = &arg[2];
        }
        else if (arg[0] == L'-') {
            fprintf(stderr, "diff3: unrecognized option '%ls'\nTry 'diff3 --help' for more information.\n", arg);
            return 2;
        } else {
            if (file_idx < 3) files[file_idx++] = arg;
            else {
                fprintf(stderr, "diff3: extra operand '%ls'\n", arg);
                return 2;
            }
        }
    }

    if (file_idx < 3) {
        fprintf(stderr, "diff3: missing operand; expected 3 files\nTry 'diff3 --help' for more information.\n");
        return 2;
    }

    ConsoleManager console;
    ConsoleManager_Init(&console);
    if (config.color_mode == COLOR_ALWAYS) config.use_color = true;
    else if (config.color_mode == COLOR_NEVER) config.use_color = false;
    else config.use_color = console.is_console;

    /* Ingest files polymorphically */
    IFileSource* f1 = MemoryMappedFileSource_Create(files[0], config.labels[0], config.strip_trailing_cr);
    IFileSource* f2 = MemoryMappedFileSource_Create(files[1], config.labels[1], config.strip_trailing_cr);
    IFileSource* f3 = MemoryMappedFileSource_Create(files[2], config.labels[2], config.strip_trailing_cr);

    if (!f1 || !f2 || !f3) {
        fprintf(stderr, "diff3: error opening files for memory mapping\n");
        if (f1) f1->vptr->Destroy(f1);
        if (f2) f2->vptr->Destroy(f2);
        if (f3) f3->vptr->Destroy(f3);
        return 2;
    }

    /* Check for Binary Contents */
    if (!config.treat_as_text) {
        if (f1->vptr->IsBinary(f1) || f2->vptr->IsBinary(f2) || f3->vptr->IsBinary(f3)) {
            fprintf(stderr, "diff3: binary file encountered (use -a to force text comparison)\n");
            f1->vptr->Destroy(f1);
            f2->vptr->Destroy(f2);
            f3->vptr->Destroy(f3);
            return 2;
        }
    }

    /* Reconcile differences */
    IDiff3Engine* engine = StandardDiff3Engine_Create();
    Diff3Script* script = engine->vptr->Reconcile(engine, f1, f2, f3);

    int exit_code = 2;
    if (script) {
        IOutputFormatter* formatter = FormatterFactory_Create(&config);
        if (formatter) {
            bool conflict = formatter->vptr->Format(formatter, f1, f2, f3, script);
            exit_code = conflict ? 1 : 0;
            formatter->vptr->Destroy(formatter);
        }
        free(script->blocks);
        free(script);
    } else {
        fprintf(stderr, "diff3: error computing 3-way differences\n");
    }

    /* Clean destruction */
    engine->vptr->Destroy(engine);
    f3->vptr->Destroy(f3);
    f2->vptr->Destroy(f2);
    f1->vptr->Destroy(f1);

    return exit_code;
}