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

#define DIFF_VERSION L"diff 3.10"
#define HASH_SEED 0xcbf29ce484222325ULL
#define HASH_PRIME 0x100000001b3ULL

/* ========================================================================= */
/* DATA STRUCTURES & CONFIGURATION                                           */
/* ========================================================================= */

typedef enum {
    FORMAT_NORMAL,
    FORMAT_UNIFIED,
    FORMAT_CONTEXT,
    FORMAT_SIDE_BY_SIDE,
    FORMAT_BRIEF
} OutputFormat;

typedef enum {
    COLOR_WHEN_NEVER,
    COLOR_WHEN_AUTO,
    COLOR_WHEN_ALWAYS
} ColorWhen;

typedef struct {
    OutputFormat format;
    int context_lines;
    int width;                  // For side-by-side
    bool ignore_case;          // -i
    bool ignore_all_space;      // -w
    bool ignore_space_change;   // -b
    bool ignore_blank_lines;    // -B
    bool strip_trailing_cr;     // --strip-trailing-cr
    bool report_identical;      // -s
    ColorWhen color_mode;       // --color
    bool use_color;             // Resolved at runtime
} DiffConfig;

typedef enum {
    OP_MATCH = 0,
    OP_DELETE,
    OP_INSERT
} OperationType;

typedef struct {
    OperationType type;
    size_t line1; // 0-based index in File 1
    size_t line2; // 0-based index in File 2
} EditOp;

typedef struct {
    EditOp* data;
    size_t count;
    size_t capacity;
} EditScript;

/* Forward Declarations */
typedef struct IFileSource IFileSource;
typedef struct IDiffEngine IDiffEngine;
typedef struct IOutputFormatter IOutputFormatter;

/* ========================================================================= */
/* CLASS: ConsoleManager (Terminal & Color Abstraction)                       */
/* ========================================================================= */

typedef struct ConsoleManager {
    HANDLE hOut;
    bool is_console;
    bool vt_enabled;
} ConsoleManager;

static void ConsoleManager_Init(ConsoleManager* self, ColorWhen color_mode) {
    self->hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    self->is_console = (GetFileType(self->hOut) == FILE_TYPE_CHAR) && GetConsoleMode(self->hOut, &mode);
    self->vt_enabled = false;

    if (self->is_console && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        if (SetConsoleMode(self->hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            self->vt_enabled = true;
        }
    } else if (self->is_console) {
        self->vt_enabled = true;
    }
}

static bool ConsoleManager_ShouldUseColor(const ConsoleManager* self, ColorWhen color_mode) {
    if (color_mode == COLOR_WHEN_ALWAYS) return true;
    if (color_mode == COLOR_WHEN_NEVER) return false;
    return self->is_console && self->vt_enabled;
}

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_MAGENTA "\x1b[35m"

/* ========================================================================= */
/* INTERFACE: IFileSource                                                    */
/* ========================================================================= */

typedef struct IFileSourceVtbl {
    void (*Destroy)(IFileSource* self);
    size_t (*GetLineCount)(const IFileSource* self);
    const char* (*GetLine)(const IFileSource* self, size_t index, size_t* out_len);
    uint64_t (*GetLineHash)(const IFileSource* self, size_t index);
    bool (*HasNoNewlineAtEOF)(const IFileSource* self);
    bool (*IsBinary)(const IFileSource* self);
    const wchar_t* (*GetPath)(const IFileSource* self);
} IFileSourceVtbl;

struct IFileSource {
    const IFileSourceVtbl* vptr;
};

/* --- Implementation: MemoryMappedFileSource --- */
typedef struct MemoryMappedFileSource {
    IFileSource base;
    wchar_t* path;
    HANDLE file_handle;
    HANDLE map_handle;
    const char* data;
    char* owned_data;
    uint64_t file_size;
    size_t* line_offsets;
    size_t* line_lengths;
    uint64_t* line_hashes;
    size_t line_count;
    bool is_binary;
    bool missing_eof_newline;
} MemoryMappedFileSource;

static uint64_t ComputeNormalizedHash(const char* str, size_t len, const DiffConfig* cfg) {
    uint64_t hash = HASH_SEED;
    size_t i = 0;
    while (i < len) {
        if (cfg->ignore_all_space && (str[i] == ' ' || str[i] == '\t')) {
            i++;
            continue;
        }
        if (cfg->ignore_space_change && (str[i] == ' ' || str[i] == '\t')) {
            while (i < len && (str[i] == ' ' || str[i] == '\t')) i++;
            unsigned char ch = ' ';
            hash ^= ch;
            hash *= HASH_PRIME;
            continue;
        }
        unsigned char ch = (unsigned char)str[i];
        if (cfg->ignore_case) ch = (unsigned char)tolower(ch);
        hash ^= ch;
        hash *= HASH_PRIME;
        i++;
    }
    return hash;
}

static void MMapFile_Destroy(IFileSource* base_self) {
    if (!base_self) return;
    MemoryMappedFileSource* self = (MemoryMappedFileSource*)base_self;
    if (self->data && !self->owned_data) UnmapViewOfFile(self->data);
    if (self->owned_data) free(self->owned_data);
    if (self->map_handle) CloseHandle(self->map_handle);
    if (self->file_handle != INVALID_HANDLE_VALUE && self->file_handle != NULL) CloseHandle(self->file_handle);
    free(self->line_offsets);
    free(self->line_lengths);
    free(self->line_hashes);
    free(self->path);
    free(self);
}

static size_t MMapFile_GetLineCount(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->line_count;
}

static const char* MMapFile_GetLine(const IFileSource* base_self, size_t index, size_t* out_len) {
    const MemoryMappedFileSource* self = (const MemoryMappedFileSource*)base_self;
    if (index >= self->line_count) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    if (out_len) *out_len = self->line_lengths[index];
    return self->data + self->line_offsets[index];
}

static uint64_t MMapFile_GetLineHash(const IFileSource* base_self, size_t index) {
    const MemoryMappedFileSource* self = (const MemoryMappedFileSource*)base_self;
    return (index < self->line_count) ? self->line_hashes[index] : 0;
}

static bool MMapFile_HasNoNewlineAtEOF(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->missing_eof_newline;
}

static bool MMapFile_IsBinary(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->is_binary;
}

static const wchar_t* MMapFile_GetPath(const IFileSource* base_self) {
    return ((const MemoryMappedFileSource*)base_self)->path;
}

static const IFileSourceVtbl g_MMapFileVtbl = {
    MMapFile_Destroy,
    MMapFile_GetLineCount,
    MMapFile_GetLine,
    MMapFile_GetLineHash,
    MMapFile_HasNoNewlineAtEOF,
    MMapFile_IsBinary,
    MMapFile_GetPath
};

IFileSource* MemoryMappedFileSource_Create(const wchar_t* path, const DiffConfig* cfg) {
    MemoryMappedFileSource* self = (MemoryMappedFileSource*)calloc(1, sizeof(MemoryMappedFileSource));
    if (!self) return NULL;
    self->base.vptr = &g_MMapFileVtbl;
    self->path = _wcsdup(path);
    self->file_handle = INVALID_HANDLE_VALUE;

    if (wcscmp(path, L"-") == 0) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        size_t cap = 65536;
        size_t total = 0;
        char* buf = (char*)malloc(cap);
        if (!buf) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }
        DWORD bytesRead = 0;
        while (ReadFile(hIn, buf + total, (DWORD)(cap - total), &bytesRead, NULL) && bytesRead > 0) {
            total += bytesRead;
            if (total == cap) {
                if (cap > SIZE_MAX / 2) {
                    free(buf);
                    MMapFile_Destroy((IFileSource*)self);
                    return NULL;
                }
                cap *= 2;
                char* new_buf = (char*)realloc(buf, cap);
                if (!new_buf) {
                    free(buf);
                    MMapFile_Destroy((IFileSource*)self);
                    return NULL;
                }
                buf = new_buf;
            }
        }
        self->owned_data = buf;
        self->data = buf;
        self->file_size = total;
    } else {
        self->file_handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (self->file_handle == INVALID_HANDLE_VALUE) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }

        LARGE_INTEGER sz;
        if (!GetFileSizeEx(self->file_handle, &sz)) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }
        self->file_size = (uint64_t)sz.QuadPart;
        if (self->file_size == 0) return (IFileSource*)self;

        self->map_handle = CreateFileMappingW(self->file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!self->map_handle) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }

        self->data = (const char*)MapViewOfFile(self->map_handle, FILE_MAP_READ, 0, 0, 0);
        if (!self->data) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }
    }

    size_t capacity = 512;
    self->line_offsets = (size_t*)malloc(capacity * sizeof(size_t));
    self->line_lengths = (size_t*)malloc(capacity * sizeof(size_t));
    self->line_hashes  = (uint64_t*)malloc(capacity * sizeof(uint64_t));

    size_t start = 0;
    for (size_t i = 0; i < self->file_size; ++i) {
        if (self->data[i] == '\0') self->is_binary = true;

        if (self->data[i] == '\n' || i == self->file_size - 1) {
            bool has_newline = (self->data[i] == '\n');
            size_t end = has_newline ? i : i + 1;
            size_t len = end - start;

            if (cfg->strip_trailing_cr && len > 0 && self->data[start + len - 1] == '\r') {
                len--;
            }

            if (cfg->ignore_blank_lines && len == 0) {
                start = i + 1;
                continue;
            }

            if (self->line_count >= capacity) {
                if (capacity > SIZE_MAX / (2 * sizeof(size_t))) {
                    MMapFile_Destroy((IFileSource*)self);
                    return NULL;
                }
                capacity *= 2;
                self->line_offsets = (size_t*)realloc(self->line_offsets, capacity * sizeof(size_t));
                self->line_lengths = (size_t*)realloc(self->line_lengths, capacity * sizeof(size_t));
                self->line_hashes  = (uint64_t*)realloc(self->line_hashes, capacity * sizeof(uint64_t));
            }

            self->line_offsets[self->line_count] = start;
            self->line_lengths[self->line_count] = len;
            self->line_hashes[self->line_count]  = ComputeNormalizedHash(self->data + start, len, cfg);
            self->line_count++;

            if (!has_newline && i == self->file_size - 1) {
                self->missing_eof_newline = true;
            }
            start = i + 1;
        }
    }

    return (IFileSource*)self;
}

IFileSource* MemoryMappedFileSource_CloneStdin(const IFileSource* src) {
    const MemoryMappedFileSource* m_src = (const MemoryMappedFileSource*)src;
    MemoryMappedFileSource* self = (MemoryMappedFileSource*)calloc(1, sizeof(MemoryMappedFileSource));
    if (!self) return NULL;
    self->base.vptr = &g_MMapFileVtbl;
    self->path = _wcsdup(m_src->path);
    self->file_handle = INVALID_HANDLE_VALUE;
    self->file_size = m_src->file_size;
    self->is_binary = m_src->is_binary;
    self->missing_eof_newline = m_src->missing_eof_newline;
    self->line_count = m_src->line_count;

    if (m_src->file_size > 0 && m_src->data) {
        self->owned_data = (char*)malloc((size_t)m_src->file_size);
        if (!self->owned_data) { MMapFile_Destroy((IFileSource*)self); return NULL; }
        memcpy(self->owned_data, m_src->data, (size_t)m_src->file_size);
        self->data = self->owned_data;
    }

    if (m_src->line_count > 0) {
        self->line_offsets = (size_t*)malloc(m_src->line_count * sizeof(size_t));
        self->line_lengths = (size_t*)malloc(m_src->line_count * sizeof(size_t));
        self->line_hashes  = (uint64_t*)malloc(m_src->line_count * sizeof(uint64_t));
        if (!self->line_offsets || !self->line_lengths || !self->line_hashes) {
            MMapFile_Destroy((IFileSource*)self);
            return NULL;
        }
        memcpy(self->line_offsets, m_src->line_offsets, m_src->line_count * sizeof(size_t));
        memcpy(self->line_lengths, m_src->line_lengths, m_src->line_count * sizeof(size_t));
        memcpy(self->line_hashes, m_src->line_hashes, m_src->line_count * sizeof(uint64_t));
    }
    return (IFileSource*)self;
}

/* ========================================================================= */
/* INTERFACE: IDiffEngine (Myers O(ND) Algorithm)                            */
/* ========================================================================= */

typedef struct IDiffEngineVtbl {
    void (*Destroy)(IDiffEngine* self);
    EditScript* (*ComputeDiff)(IDiffEngine* self, const IFileSource* src1, const IFileSource* src2);
} IDiffEngineVtbl;

struct IDiffEngine {
    const IDiffEngineVtbl* vptr;
};

typedef struct MyersDiffEngine {
    IDiffEngine base;
    DiffConfig config;
} MyersDiffEngine;

static bool Engine_LinesMatch(const IFileSource* f1, size_t idx1, const IFileSource* f2, size_t idx2, const DiffConfig* cfg) {
    if (f1->vptr->GetLineHash(f1, idx1) != f2->vptr->GetLineHash(f2, idx2)) return false;
    size_t len1, len2;
    const char* l1 = f1->vptr->GetLine(f1, idx1, &len1);
    const char* l2 = f2->vptr->GetLine(f2, idx2, &len2);

    if (cfg->ignore_all_space || cfg->ignore_space_change) {
        size_t i1 = 0, i2 = 0;
        while (i1 < len1 || i2 < len2) {
            if (cfg->ignore_all_space) {
                while (i1 < len1 && (l1[i1] == ' ' || l1[i1] == '\t')) i1++;
                while (i2 < len2 && (l2[i2] == ' ' || l2[i2] == '\t')) i2++;
            } else if (cfg->ignore_space_change) {
                bool sp1 = false, sp2 = false;
                while (i1 < len1 && (l1[i1] == ' ' || l1[i1] == '\t')) { sp1 = true; i1++; }
                while (i2 < len2 && (l2[i2] == ' ' || l2[i2] == '\t')) { sp2 = true; i2++; }
                if (sp1 != sp2) return false;
            }
            if (i1 >= len1 && i2 >= len2) return true;
            if (i1 >= len1 || i2 >= len2) return false;
            char c1 = cfg->ignore_case ? (char)tolower((unsigned char)l1[i1]) : l1[i1];
            char c2 = cfg->ignore_case ? (char)tolower((unsigned char)l2[i2]) : l2[i2];
            if (c1 != c2) return false;
            i1++; i2++;
        }
        return true;
    }

    if (len1 != len2) return false;
    return cfg->ignore_case ? (_strnicmp(l1, l2, len1) == 0) : (memcmp(l1, l2, len1) == 0);
}

static bool EditScript_Push(EditScript* script, EditOp op) {
    if (script->count >= script->capacity) {
        size_t new_cap = script->capacity ? script->capacity * 2 : 64;
        EditOp* new_data = (EditOp*)realloc(script->data, new_cap * sizeof(EditOp));
        if (!new_data) return false;
        script->data = new_data;
        script->capacity = new_cap;
    }
    script->data[script->count++] = op;
    return true;
}

static void MyersDiffEngine_Destroy(IDiffEngine* base_self) {
    free(base_self);
}

static EditScript* MyersDiffEngine_ComputeDiff(IDiffEngine* base_self, const IFileSource* f1, const IFileSource* f2) {
    MyersDiffEngine* self = (MyersDiffEngine*)base_self;
    size_t n = f1->vptr->GetLineCount(f1);
    size_t m = f2->vptr->GetLineCount(f2);
    size_t max_d = n + m;
    intptr_t offset = (intptr_t)max_d;

    if (max_d > (SIZE_MAX / 2) / sizeof(intptr_t)) return NULL;

    intptr_t* v = (intptr_t*)malloc((2 * max_d + 1) * sizeof(intptr_t));
    if (!v) return NULL;

    typedef struct { intptr_t* snap; } Trace;
    Trace* trace = (Trace*)malloc((max_d + 1) * sizeof(Trace));
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

            while (x < (intptr_t)n && y < (intptr_t)m && Engine_LinesMatch(f1, (size_t)x, f2, (size_t)y, &self->config)) {
                x++; y++;
            }
            v[offset + k] = x;
            if (x >= (intptr_t)n && y >= (intptr_t)m) goto backtrack;
        }
    }

backtrack: {
        EditScript* script = (EditScript*)calloc(1, sizeof(EditScript));
        if (!script) goto cleanup_fail;
        script->capacity = actual_d + n + m + 16;
        script->data = (EditOp*)malloc(script->capacity * sizeof(EditOp));
        if (!script->data) { free(script); goto cleanup_fail; }

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
                if (!EditScript_Push(script, (EditOp){ OP_MATCH, (size_t)cx, (size_t)cy })) {
                    free(script->data);
                    free(script);
                    goto cleanup_fail;
                }
            }
            if (cx == px) {
                cy--;
                if (!EditScript_Push(script, (EditOp){ OP_INSERT, (size_t)cx, (size_t)cy })) {
                    free(script->data);
                    free(script);
                    goto cleanup_fail;
                }
            } else {
                cx--;
                if (!EditScript_Push(script, (EditOp){ OP_DELETE, (size_t)cx, (size_t)cy })) {
                    free(script->data);
                    free(script);
                    goto cleanup_fail;
                }
            }
        }
        while (cx > 0 && cy > 0) {
            cx--; cy--;
            if (!EditScript_Push(script, (EditOp){ OP_MATCH, (size_t)cx, (size_t)cy })) {
                free(script->data);
                free(script);
                goto cleanup_fail;
            }
        }

        /* Reverse script */
        for (size_t i = 0; i < script->count / 2; ++i) {
            EditOp tmp = script->data[i];
            script->data[i] = script->data[script->count - 1 - i];
            script->data[script->count - 1 - i] = tmp;
        }

        for (size_t d = 0; d <= actual_d; ++d) free(trace[d].snap);
        free(trace);
        free(v);
        return script;
    }

cleanup_fail:
    for (size_t d = 0; d <= actual_d; ++d) free(trace[d].snap);
    free(trace);
    free(v);
    return NULL;
}

static const IDiffEngineVtbl g_MyersDiffEngineVtbl = {
    MyersDiffEngine_Destroy,
    MyersDiffEngine_ComputeDiff
};

IDiffEngine* MyersDiffEngine_Create(const DiffConfig* cfg) {
    MyersDiffEngine* self = (MyersDiffEngine*)malloc(sizeof(MyersDiffEngine));
    if (!self) return NULL;
    self->base.vptr = &g_MyersDiffEngineVtbl;
    self->config = *cfg;
    return (IDiffEngine*)self;
}

/* ========================================================================= */
/* INTERFACE: IOutputFormatter                                               */
/* ========================================================================= */

typedef struct IOutputFormatterVtbl {
    void (*Destroy)(IOutputFormatter* self);
    void (*Format)(IOutputFormatter* self, const IFileSource* f1, const IFileSource* f2, const EditScript* script);
} IOutputFormatterVtbl;

struct IOutputFormatter {
    const IOutputFormatterVtbl* vptr;
};

/* --- Common Output Utility Functions --- */
static void SafePrintRawLine(const char* line, size_t len, const char* prefix, const char* color, bool use_color) {
    if (use_color && color) printf("%s", color);
    if (prefix) printf("%s", prefix);
    fwrite(line, 1, len, stdout);
    if (use_color && color) printf(ANSI_RESET);
    putchar('\n');
}

static void CheckPrintMissingNewline(const IFileSource* f, size_t idx) {
    if (idx + 1 == f->vptr->GetLineCount(f) && f->vptr->HasNoNewlineAtEOF(f)) {
        printf("\\ No newline at end of file\n");
    }
}

/* --- Concrete Implementation: UnifiedDiffFormatter (-u, -U) --- */
typedef struct UnifiedDiffFormatter {
    IOutputFormatter base;
    DiffConfig config;
} UnifiedDiffFormatter;

static void Unified_Destroy(IOutputFormatter* self) { free(self); }

static void Unified_Format(IOutputFormatter* base_self, const IFileSource* f1, const IFileSource* f2, const EditScript* script) {
    UnifiedDiffFormatter* self = (UnifiedDiffFormatter*)base_self;
    if (self->config.use_color) printf(ANSI_BOLD);
    wprintf(L"--- %ls\n", f1->vptr->GetPath(f1));
    wprintf(L"+++ %ls\n", f2->vptr->GetPath(f2));
    if (self->config.use_color) printf(ANSI_RESET);

    size_t i = 0;
    while (i < script->count) {
        while (i < script->count && script->data[i].type == OP_MATCH) i++;
        if (i >= script->count) break;

        size_t h_start = (i >= (size_t)self->config.context_lines) ? i - self->config.context_lines : 0;
        size_t h_end = i;

        while (h_end < script->count) {
            if (script->data[h_end].type != OP_MATCH) {
                h_end++;
            } else {
                size_t look = h_end;
                while (look < script->count && script->data[look].type == OP_MATCH) look++;
                if (look < script->count && (look - h_end) <= (size_t)(self->config.context_lines * 2)) {
                    h_end = look;
                } else {
                    h_end = (h_end + self->config.context_lines < script->count) ? h_end + self->config.context_lines : script->count;
                    break;
                }
            }
        }

        size_t s1 = script->data[h_start].line1 + 1, s2 = script->data[h_start].line2 + 1;
        size_t c1 = 0, c2 = 0;
        for (size_t j = h_start; j < h_end; ++j) {
            if (script->data[j].type == OP_MATCH || script->data[j].type == OP_DELETE) c1++;
            if (script->data[j].type == OP_MATCH || script->data[j].type == OP_INSERT) c2++;
        }

        if (self->config.use_color) printf(ANSI_CYAN);
        printf("@@ -%zu,%zu +%zu,%zu @@\n", s1, c1, s2, c2);
        if (self->config.use_color) printf(ANSI_RESET);

        for (size_t j = h_start; j < h_end; ++j) {
            size_t len;
            const char* line;
            switch (script->data[j].type) {
                case OP_MATCH:
                    line = f1->vptr->GetLine(f1, script->data[j].line1, &len);
                    SafePrintRawLine(line, len, " ", NULL, false);
                    break;
                case OP_DELETE:
                    line = f1->vptr->GetLine(f1, script->data[j].line1, &len);
                    SafePrintRawLine(line, len, "-", ANSI_RED, self->config.use_color);
                    CheckPrintMissingNewline(f1, script->data[j].line1);
                    break;
                case OP_INSERT:
                    line = f2->vptr->GetLine(f2, script->data[j].line2, &len);
                    SafePrintRawLine(line, len, "+", ANSI_GREEN, self->config.use_color);
                    CheckPrintMissingNewline(f2, script->data[j].line2);
                    break;
            }
        }
        i = h_end;
    }
}

static const IOutputFormatterVtbl g_UnifiedFormatterVtbl = { Unified_Destroy, Unified_Format };

/* --- Concrete Implementation: NormalDiffFormatter (Classic POSIX ed-like) --- */
typedef struct NormalDiffFormatter {
    IOutputFormatter base;
    DiffConfig config;
} NormalDiffFormatter;

static void Normal_Destroy(IOutputFormatter* self) { free(self); }

static void Normal_Format(IOutputFormatter* base_self, const IFileSource* f1, const IFileSource* f2, const EditScript* script) {
    NormalDiffFormatter* self = (NormalDiffFormatter*)base_self;
    size_t i = 0;
    while (i < script->count) {
        while (i < script->count && script->data[i].type == OP_MATCH) i++;
        if (i >= script->count) break;

        size_t del_start = 0, del_count = 0, ins_start = 0, ins_count = 0;
        size_t start_i = i;

        while (i < script->count && script->data[i].type == OP_DELETE) {
            if (del_count == 0) del_start = script->data[i].line1 + 1;
            del_count++;
            i++;
        }
        while (i < script->count && script->data[i].type == OP_INSERT) {
            if (ins_count == 0) ins_start = script->data[i].line2 + 1;
            ins_count++;
            i++;
        }

        char mode = (del_count > 0 && ins_count > 0) ? 'c' : ((del_count > 0) ? 'd' : 'a');
        if (self->config.use_color) printf(ANSI_BOLD);

        if (del_count <= 1) printf("%zu", (del_count == 0) ? script->data[start_i].line1 : del_start);
        else printf("%zu,%zu", del_start, del_start + del_count - 1);

        printf("%c", mode);

        if (ins_count <= 1) printf("%zu\n", (ins_count == 0) ? script->data[start_i].line2 : ins_start);
        else printf("%zu,%zu\n", ins_start, ins_start + ins_count - 1);

        if (self->config.use_color) printf(ANSI_RESET);

        for (size_t j = start_i; j < start_i + del_count; ++j) {
            size_t len;
            const char* l = f1->vptr->GetLine(f1, script->data[j].line1, &len);
            SafePrintRawLine(l, len, "< ", ANSI_RED, self->config.use_color);
            CheckPrintMissingNewline(f1, script->data[j].line1);
        }

        if (del_count > 0 && ins_count > 0) printf("---\n");

        for (size_t j = start_i + del_count; j < start_i + del_count + ins_count; ++j) {
            size_t len;
            const char* l = f2->vptr->GetLine(f2, script->data[j].line2, &len);
            SafePrintRawLine(l, len, "> ", ANSI_GREEN, self->config.use_color);
            CheckPrintMissingNewline(f2, script->data[j].line2);
        }
    }
}

static const IOutputFormatterVtbl g_NormalFormatterVtbl = { Normal_Destroy, Normal_Format };

/* --- Concrete Implementation: SideBySideDiffFormatter (-y) --- */
typedef struct SideBySideDiffFormatter {
    IOutputFormatter base;
    DiffConfig config;
} SideBySideDiffFormatter;

static void SideBySide_Destroy(IOutputFormatter* self) { free(self); }

static void PrintPadded(const char* str, size_t len, int target_w) {
    int printed = 0;
    for (size_t i = 0; i < len && printed < target_w; ++i) {
        if (str[i] == '\t') {
            int spaces = 8 - (printed % 8);
            while (spaces-- && printed < target_w) { putchar(' '); printed++; }
        } else {
            putchar(str[i]);
            printed++;
        }
    }
    while (printed < target_w) { putchar(' '); printed++; }
}

static void SideBySide_Format(IOutputFormatter* base_self, const IFileSource* f1, const IFileSource* f2, const EditScript* script) {
    SideBySideDiffFormatter* self = (SideBySideDiffFormatter*)base_self;
    int half = (self->config.width - 3) / 2;
    if (half < 10) half = 10;

    size_t i = 0;
    while (i < script->count) {
        if (script->data[i].type == OP_MATCH) {
            size_t len;
            const char* l = f1->vptr->GetLine(f1, script->data[i].line1, &len);
            PrintPadded(l, len, half);
            printf("   ");
            fwrite(l, 1, (len > (size_t)half) ? (size_t)half : len, stdout);
            putchar('\n');
            i++;
        } else if (i + 1 < script->count && script->data[i].type == OP_DELETE && script->data[i+1].type == OP_INSERT) {
            size_t l1, l2;
            const char* line1 = f1->vptr->GetLine(f1, script->data[i].line1, &l1);
            const char* line2 = f2->vptr->GetLine(f2, script->data[i+1].line2, &l2);
            if (self->config.use_color) printf(ANSI_MAGENTA);
            PrintPadded(line1, l1, half);
            printf(" | ");
            fwrite(line2, 1, (l2 > (size_t)half) ? (size_t)half : l2, stdout);
            if (self->config.use_color) printf(ANSI_RESET);
            putchar('\n');
            i += 2;
        } else if (script->data[i].type == OP_DELETE) {
            size_t l1;
            const char* line1 = f1->vptr->GetLine(f1, script->data[i].line1, &l1);
            if (self->config.use_color) printf(ANSI_RED);
            PrintPadded(line1, l1, half);
            printf(" <\n");
            if (self->config.use_color) printf(ANSI_RESET);
            i++;
        } else if (script->data[i].type == OP_INSERT) {
            size_t l2;
            const char* line2 = f2->vptr->GetLine(f2, script->data[i].line2, &l2);
            if (self->config.use_color) printf(ANSI_GREEN);
            PrintPadded("", 0, half);
            printf(" > ");
            fwrite(line2, 1, (l2 > (size_t)half) ? (size_t)half : l2, stdout);
            if (self->config.use_color) printf(ANSI_RESET);
            putchar('\n');
            i++;
        }
    }
}

static const IOutputFormatterVtbl g_SideBySideFormatterVtbl = { SideBySide_Destroy, SideBySide_Format };

/* --- Concrete Implementation: BriefFormatter (-q) --- */
typedef struct BriefFormatter {
    IOutputFormatter base;
} BriefFormatter;

static void Brief_Destroy(IOutputFormatter* self) { free(self); }
static void Brief_Format(IOutputFormatter* self, const IFileSource* f1, const IFileSource* f2, const EditScript* script) {
    (void)self;
    for (size_t i = 0; i < script->count; ++i) {
        if (script->data[i].type != OP_MATCH) {
            wprintf(L"Files %ls and %ls differ\n", f1->vptr->GetPath(f1), f2->vptr->GetPath(f2));
            return;
        }
    }
}
static const IOutputFormatterVtbl g_BriefFormatterVtbl = { Brief_Destroy, Brief_Format };

/* Factory for Formatters */
IOutputFormatter* FormatterFactory_Create(const DiffConfig* cfg) {
    if (cfg->format == FORMAT_BRIEF) {
        BriefFormatter* b = (BriefFormatter*)malloc(sizeof(BriefFormatter));
        if (b) b->base.vptr = &g_BriefFormatterVtbl;
        return (IOutputFormatter*)b;
    } else if (cfg->format == FORMAT_UNIFIED) {
        UnifiedDiffFormatter* u = (UnifiedDiffFormatter*)malloc(sizeof(UnifiedDiffFormatter));
        if (u) { u->base.vptr = &g_UnifiedFormatterVtbl; u->config = *cfg; }
        return (IOutputFormatter*)u;
    } else if (cfg->format == FORMAT_SIDE_BY_SIDE) {
        SideBySideDiffFormatter* s = (SideBySideDiffFormatter*)malloc(sizeof(SideBySideDiffFormatter));
        if (s) { s->base.vptr = &g_SideBySideFormatterVtbl; s->config = *cfg; }
        return (IOutputFormatter*)s;
    }
    NormalDiffFormatter* n = (NormalDiffFormatter*)malloc(sizeof(NormalDiffFormatter));
    if (n) { n->base.vptr = &g_NormalFormatterVtbl; n->config = *cfg; }
    return (IOutputFormatter*)n;
}

/* ========================================================================= */
/* CLI PARSER & HELP SYSTEM                                                  */
/* ========================================================================= */

static bool ParsePositiveInt(const wchar_t* str, int* out_val) {
    if (!str || !*str) return false;
    wchar_t* end = NULL;
    long val = wcstol(str, &end, 10);
    if (!end || *end != L'\0' || val < 0 || val > 1000000) return false;
    *out_val = (int)val;
    return true;
}

static void DisplayHelp(void) {
    wprintf(L"Usage: diff [OPTION]... FILES\n");
    wprintf(L"Compare FILES line by line.\n\n");
    wprintf(L"Mandatory arguments to long options are mandatory for short options too.\n");
    wprintf(L"  -c, -C NUM, --context[=NUM]   output NUM (default 3) lines of copied context\n");
    wprintf(L"  -u, -U NUM, --unified[=NUM]   output NUM (default 3) lines of unified context\n");
    wprintf(L"  -e, --ed                      output an ed script (not supported in current vtbl)\n");
    wprintf(L"  -q, --brief                   report only when files differ\n");
    wprintf(L"  -y, --side-by-side            output in two columns\n");
    wprintf(L"  -W, --width=NUM               output at most NUM (default 130) print columns\n");
    wprintf(L"  -i, --ignore-case             ignore case differences in file contents\n");
    wprintf(L"  -b, --ignore-space-change     ignore changes in the amount of white space\n");
    wprintf(L"  -w, --ignore-all-space        ignore all white space\n");
    wprintf(L"  -B, --ignore-blank-lines      ignore changes where lines are all blank\n");
    wprintf(L"  -s, --report-identical-files  report when two files are the same\n");
    wprintf(L"      --strip-trailing-cr       strip trailing carriage return on input\n");
    wprintf(L"      --color[=WHEN]            colorize the output; WHEN can be 'never', 'always',\n");
    wprintf(L"                                  or 'auto' (the default)\n");
    wprintf(L"      --help                    display this help and exit\n");
    wprintf(L"  -v, --version                 output version information and exit\n\n");
    wprintf(L"Exit status is 0 if inputs are the same, 1 if different, 2 if trouble.\n");
}

static void DisplayVersion(void) {
    wprintf(L"%ls\n", DIFF_VERSION);
    wprintf(L"Engine: Fast 64-bit Myers Difference Algorithm (Zero-Copy Win32 MMF)\n");
}

/* ========================================================================= */
/* MAIN APPLICATION CONTROLLER                                               */
/* ========================================================================= */

int wmain(int argc, wchar_t* argv[]) {
    DiffConfig config = {
        .format = FORMAT_NORMAL,
        .context_lines = 3,
        .width = 130,
        .ignore_case = false,
        .ignore_all_space = false,
        .ignore_space_change = false,
        .ignore_blank_lines = false,
        .strip_trailing_cr = false,
        .report_identical = false,
        .color_mode = COLOR_WHEN_AUTO,
        .use_color = false
    };

    wchar_t* file1_path = NULL;
    wchar_t* file2_path = NULL;
    bool end_of_options = false;

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];
        if (end_of_options) {
            if (!file1_path) file1_path = arg;
            else if (!file2_path) file2_path = arg;
            else {
                fwprintf(stderr, L"diff: extra operand '%ls'\n", arg);
                return 2;
            }
            continue;
        }

        if (wcscmp(arg, L"--") == 0) {
            end_of_options = true;
            continue;
        }
        if (wcscmp(arg, L"--help") == 0) { DisplayHelp(); return 0; }
        if (wcscmp(arg, L"-v") == 0 || wcscmp(arg, L"--version") == 0) { DisplayVersion(); return 0; }
        if (wcscmp(arg, L"-u") == 0) { config.format = FORMAT_UNIFIED; }
        else if (wcsncmp(arg, L"-U", 2) == 0) {
            config.format = FORMAT_UNIFIED;
            const wchar_t* val_str = (arg[2] != L'\0') ? &arg[2] : ((i + 1 < argc) ? argv[++i] : NULL);
            if (!ParsePositiveInt(val_str, &config.context_lines)) {
                fwprintf(stderr, L"diff: option '-U' requires a non-negative integer argument\n");
                return 2;
            }
        }
        else if (wcscmp(arg, L"-c") == 0) { config.format = FORMAT_CONTEXT; }
        else if (wcsncmp(arg, L"-C", 2) == 0) {
            config.format = FORMAT_CONTEXT;
            const wchar_t* val_str = (arg[2] != L'\0') ? &arg[2] : ((i + 1 < argc) ? argv[++i] : NULL);
            if (!ParsePositiveInt(val_str, &config.context_lines)) {
                fwprintf(stderr, L"diff: option '-C' requires a non-negative integer argument\n");
                return 2;
            }
        }
        else if (wcscmp(arg, L"-q") == 0 || wcscmp(arg, L"--brief") == 0) { config.format = FORMAT_BRIEF; }
        else if (wcscmp(arg, L"-y") == 0 || wcscmp(arg, L"--side-by-side") == 0) { config.format = FORMAT_SIDE_BY_SIDE; }
        else if (wcscmp(arg, L"-i") == 0 || wcscmp(arg, L"--ignore-case") == 0) { config.ignore_case = true; }
        else if (wcscmp(arg, L"-w") == 0 || wcscmp(arg, L"--ignore-all-space") == 0) { config.ignore_all_space = true; }
        else if (wcscmp(arg, L"-b") == 0 || wcscmp(arg, L"--ignore-space-change") == 0) { config.ignore_space_change = true; }
        else if (wcscmp(arg, L"-B") == 0 || wcscmp(arg, L"--ignore-blank-lines") == 0) { config.ignore_blank_lines = true; }
        else if (wcscmp(arg, L"-s") == 0 || wcscmp(arg, L"--report-identical-files") == 0) { config.report_identical = true; }
        else if (wcscmp(arg, L"--strip-trailing-cr") == 0) { config.strip_trailing_cr = true; }
        else if (wcsncmp(arg, L"--color", 7) == 0) {
            if (arg[7] == L'=') {
                if (wcscmp(&arg[8], L"always") == 0) config.color_mode = COLOR_WHEN_ALWAYS;
                else if (wcscmp(&arg[8], L"never") == 0) config.color_mode = COLOR_WHEN_NEVER;
                else config.color_mode = COLOR_WHEN_AUTO;
            } else config.color_mode = COLOR_WHEN_ALWAYS;
        }
        else if (wcsncmp(arg, L"-W", 2) == 0 || wcsncmp(arg, L"--width=", 8) == 0) {
            const wchar_t* num = (wcsncmp(arg, L"-W", 2) == 0) ? (arg[2] ? &arg[2] : argv[++i]) : &arg[8];
            if (!ParsePositiveInt(num, &config.width)) {
                fwprintf(stderr, L"diff: option '--width' requires a positive integer argument\n");
                return 2;
            }
        }
        else if (arg[0] == L'-' && arg[1] != L'\0') {
            fwprintf(stderr, L"diff: unrecognized option '%ls'\nTry 'diff --help' for more information.\n", arg);
            return 2;
        } else {
            if (!file1_path) file1_path = arg;
            else if (!file2_path) file2_path = arg;
            else {
                fwprintf(stderr, L"diff: extra operand '%ls'\n", arg);
                return 2;
            }
        }
    }

    if (!file1_path || !file2_path) {
        fwprintf(stderr, L"diff: missing operand after '%ls'\nTry 'diff --help' for more information.\n", argc > 1 ? argv[argc-1] : L"diff");
        return 2;
    }

    ConsoleManager console;
    ConsoleManager_Init(&console, config.color_mode);
    config.use_color = ConsoleManager_ShouldUseColor(&console, config.color_mode);

    /* Polymorphic Ingestion */
    IFileSource* file1 = MemoryMappedFileSource_Create(file1_path, &config);
    if (!file1) { fwprintf(stderr, L"diff: cannot access '%ls'\n", file1_path); return 2; }

    IFileSource* file2 = NULL;
    if (wcscmp(file1_path, L"-") == 0 && wcscmp(file2_path, L"-") == 0) {
        file2 = MemoryMappedFileSource_CloneStdin(file1);
    } else {
        file2 = MemoryMappedFileSource_Create(file2_path, &config);
    }
    if (!file2) {
        fwprintf(stderr, L"diff: cannot access '%ls'\n", file2_path);
        file1->vptr->Destroy(file1);
        return 2;
    }

    /* Binary File Safety Guard */
    if (file1->vptr->IsBinary(file1) || file2->vptr->IsBinary(file2)) {
        const MemoryMappedFileSource* m1 = (const MemoryMappedFileSource*)file1;
        const MemoryMappedFileSource* m2 = (const MemoryMappedFileSource*)file2;
        bool identical = (m1->file_size == m2->file_size);
        if (identical && m1->file_size > 0) {
            identical = (memcmp(m1->data, m2->data, (size_t)m1->file_size) == 0);
        }
        if (!identical) {
            wprintf(L"Binary files %ls and %ls differ\n", file1->vptr->GetPath(file1), file2->vptr->GetPath(file2));
            file1->vptr->Destroy(file1);
            file2->vptr->Destroy(file2);
            return 1;
        } else {
            if (config.report_identical) {
                wprintf(L"Files %ls and %ls are identical\n", file1->vptr->GetPath(file1), file2->vptr->GetPath(file2));
            }
            file1->vptr->Destroy(file1);
            file2->vptr->Destroy(file2);
            return 0;
        }
    }

    /* Engine Computation */
    IDiffEngine* engine = MyersDiffEngine_Create(&config);
    if (!engine) {
        file1->vptr->Destroy(file1);
        file2->vptr->Destroy(file2);
        return 2;
    }

    EditScript* script = engine->vptr->ComputeDiff(engine, file1, file2);
    if (!script) {
        fwprintf(stderr, L"diff: memory exhaustion during graph traversal\n");
        engine->vptr->Destroy(engine);
        file1->vptr->Destroy(file1);
        file2->vptr->Destroy(file2);
        return 2;
    }

    bool has_differences = false;
    for (size_t i = 0; i < script->count; ++i) {
        if (script->data[i].type != OP_MATCH) {
            has_differences = true;
            break;
        }
    }

    /* Rendering */
    if (has_differences) {
        IOutputFormatter* formatter = FormatterFactory_Create(&config);
        if (formatter) {
            formatter->vptr->Format(formatter, file1, file2, script);
            formatter->vptr->Destroy(formatter);
        }
    } else if (config.report_identical) {
        wprintf(L"Files %ls and %ls are identical\n", file1->vptr->GetPath(file1), file2->vptr->GetPath(file2));
    }

    /* Structured Teardown */
    free(script->data);
    free(script);
    engine->vptr->Destroy(engine);
    file2->vptr->Destroy(file2);
    file1->vptr->Destroy(file1);

    return has_differences ? 1 : 0;
}