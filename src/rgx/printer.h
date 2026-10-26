#ifndef RGX_PRINTER_H
#define RGX_PRINTER_H

#include "options.h"
#include "matcher.h"

typedef struct RgxPrinter {
    const RgxOptions* opts;
    bool enable_color;
    HANDLE hOut;
    FILE* stream;
    size_t line_offset;
    bool file_emitted, heading_emitted;
    uint64_t files, bytes, lines, matched_lines, matches;
    uint64_t file_bytes, file_lines, file_matched_lines, file_matches;
    LARGE_INTEGER started, frequency;
    IMatcher* matcher;
    bool line_terminated;
    uint64_t files_with_matches;
    uint64_t bytes_printed, file_output_start;
    LARGE_INTEGER file_started;
} RgxPrinter;
RgxPrinter* RgxPrinter_New(const RgxOptions* opts);
void RgxPrinter_PrintPath(RgxPrinter* self, const char* path);
void RgxPrinter_PrintCount(RgxPrinter* self, const char* path, size_t count, bool print_path);
void RgxPrinter_PrintMatch(RgxPrinter* self, const char* path, size_t line_num, const char* line, size_t len, MatchSpan span, bool print_path);
void RgxPrinter_PrintContext(RgxPrinter* self, const char* path, size_t line_num, const char* line, size_t len, bool print_path);
void RgxPrinter_Free(RgxPrinter* self);
void printer_begin_file(RgxPrinter* printer,const char* path);
void printer_end_file(RgxPrinter* printer,const char* path);
void printer_finish(RgxPrinter* printer);
void printer_merge(RgxPrinter* target,const RgxPrinter* source);

#endif
