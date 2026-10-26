#include "file_search.h"
#include "allocation.h"
#include "unicode.h"
#include "search_state.h"
#include "text_reader.h"
#include "path_conversion.h"
#include "multiline_search.h"

typedef struct { char* text; size_t length, number, offset; bool terminated; } ContextLine;

static void context_emit(RgxPrinter* printer, const char* path, ContextLine line, bool print_path, size_t* last_printed, bool match, MatchSpan span) {
    if (line.number <= *last_printed) return;
    printer->line_offset=line.offset;
    printer->line_terminated=line.terminated;
    if (*last_printed && line.number > *last_printed + 1 && !printer->opts->json) fprintf(printer->stream,"--\n");
    if (match) RgxPrinter_PrintMatch(printer, path, line.number, line.text, line.length, span, print_path);
    else RgxPrinter_PrintContext(printer, path, line.number, line.text, line.length, print_path);
    *last_printed = line.number;
}

static void search_stream(FILE* stream, const char* path, const RgxOptions* opts, IMatcher* matcher, RgxPrinter* printer, bool multiple_targets) {
    if(opts->multiline){search_multiline(stream,path,opts,matcher,printer,multiple_targets);return;}
    printer_begin_file(printer,path);
    printer->matcher=matcher;
    size_t byte_offset=0;
    TextReader reader; reader_init(&reader, stream); rgx_match_error = false;
    size_t context_capacity = opts->count_only || opts->quiet || opts->only_matching || opts->files_with_matches || opts->files_without_match ? 0 : opts->before_context;
    ContextLine* history = context_capacity ? (ContextLine*)rgx_calloc(context_capacity, sizeof(ContextLine)) : NULL;
    char* line = (char*)rgx_malloc(4096); size_t capacity = 4096, length = 0, number = 0, selected_count = 0, occurrence_count = 0, history_bytes = 0;
    size_t head = 0, stored = 0, last_printed = 0, after_remaining = 0;
    bool limited = opts->max_count_set && opts->max_count == 0;
    bool print_path = opts->filename_mode >= 0 ? opts->filename_mode != 0 : multiple_targets;
    while ((!limited || after_remaining) && reader_line(&reader, &line, &capacity, &length)) {
        ++number;
        size_t line_offset=byte_offset;
        size_t consumed=length+(reader.line_terminated?1:0);byte_offset+=consumed;
        printer->line_offset=line_offset;printer->bytes+=consumed;printer->file_bytes+=consumed;++printer->lines;++printer->file_lines;
        printer->line_terminated=reader.line_terminated;
        if (opts->crlf && length && line[length - 1] == '\r') --length;
        if (!opts->text && length && memchr(line, 0, length)) break;
        MatchSpan span = {0}; bool raw_match = !limited && matcher->vptr->FindMatch(matcher, line, length, 0, &span);
        if (rgx_match_error) break;
        if (opts->line_regexp && raw_match && (span.start != 0 || span.length != length)) raw_match = false;
        bool selected = !limited && (opts->invert_match ? !raw_match : raw_match);
        if (selected) {
            ++selected_count;
            ++printer->matched_lines;++printer->file_matched_lines;
            if (!opts->files_without_match) rgx_found = true;
            if (opts->quiet || opts->files_without_match) break;
            if (opts->files_with_matches) { RgxPrinter_PrintPath(printer, path); break; }
            MatchSpan first_span=span;
            if (opts->only_matching || opts->count_matches || opts->stats || opts->json) {
                if (!opts->invert_match) {
                    size_t offset = 0;
                    do {
                        ++occurrence_count;
                        ++printer->matches;++printer->file_matches;
                        if (span.length && opts->only_matching && !opts->count_only) RgxPrinter_PrintMatch(printer, path, number, line, length, span, print_path);
                        offset = span.start + span.length;
                        if (!span.length) { if (offset == length) break; size_t width; utf8_rune(line + offset, length - offset, &width); offset += width; }
                    } while (offset <= length && matcher->vptr->FindMatch(matcher, line, length, offset, &span) && !rgx_match_error);
                }
            }
            span=first_span;
            if (!opts->only_matching && !opts->count_only) {
                if (opts->before_context || opts->after_context) {
                    for (size_t index = 0; index < stored; ++index) context_emit(printer, path, history[(head + index) % opts->before_context], print_path, &last_printed, false, (MatchSpan){0});
                    context_emit(printer, path, (ContextLine){line, length, number,line_offset,reader.line_terminated}, print_path, &last_printed, true, opts->invert_match ? (MatchSpan){0} : span);
                } else RgxPrinter_PrintMatch(printer, path, number, line, length, opts->invert_match ? (MatchSpan){0} : span, print_path);
                after_remaining = opts->after_context;
            }
            if (opts->max_count_set && selected_count >= opts->max_count) limited = true;
        } else if (after_remaining && !opts->count_only && !opts->only_matching) {
            context_emit(printer, path, (ContextLine){line, length, number,line_offset,reader.line_terminated}, print_path, &last_printed, false, span); --after_remaining;
        }
        if (history) {
            size_t slot = stored < opts->before_context ? (head + stored++) % opts->before_context : head;
            if (stored == opts->before_context && history[slot].text) head = (head + 1) % opts->before_context;
            history_bytes -= history[slot].length;
            if (length > 64U * 1024U * 1024U - history_bytes) { fprintf(stderr, "rgx: before-context exceeds 64 MiB safety limit\n"); rgx_had_error = true; break; }
            history_bytes += length;
            free(history[slot].text); history[slot].text = (char*)rgx_malloc(length ? length : 1);
            if (length) memcpy(history[slot].text, line, length);
            history[slot].length = length; history[slot].number = number;
            history[slot].offset=line_offset;
            history[slot].terminated=reader.line_terminated;
        }
        if(rgx_match_error)break;
    }
    if (ferror(stream)) { fprintf(stderr, "rgx: read error: %s\n", path); rgx_had_error = true; }
    if (opts->files_without_match && !selected_count) { rgx_found = true; if (!opts->quiet) RgxPrinter_PrintPath(printer, path); }
    if (opts->count_only && selected_count && !opts->quiet && !opts->files_with_matches && !opts->files_without_match) RgxPrinter_PrintCount(printer, path, (opts->count_matches || opts->only_matching) && !opts->invert_match ? occurrence_count : selected_count, print_path);
    for (size_t index = 0; index < context_capacity; ++index) free(history[index].text);
    free(history); free(line);
    if(!opts->only_matching && !opts->count_matches && !opts->stats && !opts->json){printer->matches+=selected_count;printer->file_matches+=selected_count;}
    printer_end_file(printer,path);
}

void SearchSingleFile(const char* filepath, const RgxOptions* opts, IMatcher* matcher, RgxPrinter* printer, bool multiple_targets) {
    if (strcmp(filepath, "-") == 0) { search_stream(stdin, "<stdin>", opts, matcher, printer, multiple_targets); return; }
    wchar_t* path = path_to_wide(filepath); if (!path) return;
    FILE* stream = NULL; int error = _wfopen_s(&stream, path, L"rb"); free(path);
    if (error || !stream) { char message[256] = {0}; strerror_s(message, sizeof(message), error); fprintf(stderr, "rgx: cannot read '%s': %s\n", filepath, message); rgx_had_error = true; return; }
    search_stream(stream, filepath, opts, matcher, printer, multiple_targets); fclose(stream);
}

/* ========================================================================= */
/*                          5. OOP DIRECTORY WALKER                          */
/* ========================================================================= */
