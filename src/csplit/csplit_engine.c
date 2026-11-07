/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 * See the repository license file for the full license text.
 */

#include "csplit_engine.h"

#include "dynamic_string.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct OutputLine {
    size_t line_number;
    char* data;
    size_t length;
    size_t split_count;
    bool skip;
    struct OutputLine* next;
} OutputLine;

typedef struct {
    OutputLine* head;
    OutputLine* tail;
    size_t count;
} OutputLineQueue;

static bool OutputLineQueue_append(OutputLineQueue* queue, const DynamicString* line,
                                   size_t line_number, bool skip, size_t split_count) {
    if (queue->count == SIZE_MAX || line->length == SIZE_MAX) return false;
    OutputLine* item = (OutputLine*)calloc(1, sizeof(OutputLine));
    if (!item) return false;
    item->data = (char*)malloc(line->length + 1);
    if (!item->data) {
        free(item);
        return false;
    }

    memcpy(item->data, line->buffer, line->length);
    item->data[line->length] = '\0';
    item->line_number = line_number;
    item->length = line->length;
    item->skip = skip;
    item->split_count = split_count;
    if (queue->tail) queue->tail->next = item;
    else queue->head = item;
    queue->tail = item;
    queue->count++;
    return true;
}

static bool OutputLineQueue_mark_split(OutputLineQueue* queue, size_t line_number) {
    for (OutputLine* item = queue->head; item; item = item->next) {
        if (item->line_number != line_number) continue;
        if (item->split_count == SIZE_MAX) return false;
        item->split_count++;
        return true;
    }
    return false;
}

static bool OutputLineQueue_write_oldest(OutputLineQueue* queue,
                                         OutputFileManager* output) {
    OutputLine* item = queue->head;
    if (!item) return true;

    for (size_t i = 0; i < item->split_count; ++i) {
        if (!OutputFileManager_open_next_file(output)) return false;
    }
    if (!item->skip &&
        !OutputFileManager_write_line(output, item->data, item->length)) {
        return false;
    }

    queue->head = item->next;
    if (!queue->head) queue->tail = NULL;
    queue->count--;
    free(item->data);
    free(item);
    return true;
}

static void OutputLineQueue_destroy(OutputLineQueue* queue) {
    while (queue->head) {
        OutputLine* item = queue->head;
        queue->head = item->next;
        free(item->data);
        free(item);
    }
    queue->tail = NULL;
    queue->count = 0;
}

static int Input_read_line(FILE* input, DynamicString* line) {
    DynamicString_clear(line);
    bool read_any = false;
    int character;

    while ((character = fgetc(input)) != EOF) {
        read_any = true;
        if (!DynamicString_append_char(line, (char)character)) {
            fprintf(stderr, "csplit: out of memory while reading input\n");
            return -1;
        }
        if (character == '\n') break;
    }
    if (ferror(input)) {
        fprintf(stderr, "csplit: error reading input\n");
        return -1;
    }
    return read_any ? 1 : 0;
}

static void CSplitEngine_advance_pattern(Pattern* pattern, size_t* pattern_index,
                                         long long* repeat_counter) {
    if (pattern->repeat_count == -1) return;
    if (*repeat_counter < pattern->repeat_count) {
        (*repeat_counter)++;
    } else {
        (*pattern_index)++;
        *repeat_counter = 0;
    }
}

bool CSplitEngine_run(FILE* input, Pattern* const* patterns, size_t pattern_count,
                      size_t max_lookbehind, OutputFileManager* output) {
    DynamicString line;
    DynamicString_init(&line);
    OutputLineQueue queue = { 0 };
    size_t current_line_number = 1;
    size_t current_pattern_index = 0;
    long long repeat_counter = 0;
    size_t pending_split_line = 0;

    if (!OutputFileManager_open_next_file(output)) {
        DynamicString_destroy(&line);
        return false;
    }

    for (;;) {
        int read_result = Input_read_line(input, &line);
        if (read_result < 0) goto failure;
        if (read_result == 0) break;

        size_t split_count = 0;
        bool skip_line = false;
        bool pattern_blocked = false;

        if (pending_split_line != 0) {
            if (current_line_number == pending_split_line) {
                split_count++;
                pending_split_line = 0;
                pattern_blocked = true;
            } else if (current_line_number < pending_split_line) {
                pattern_blocked = true;
            } else {
                fprintf(stderr, "csplit: internal error resolving pattern offset\n");
                goto failure;
            }
        }

        if (!pattern_blocked && current_pattern_index < pattern_count) {
            Pattern* pattern = patterns[current_pattern_index];
            size_t match_length = line.length;
            while (match_length > 0 &&
                   (line.buffer[match_length - 1] == '\r' ||
                    line.buffer[match_length - 1] == '\n')) {
                match_length--;
            }

            char saved_character = line.buffer[match_length];
            line.buffer[match_length] = '\0';
            int matched = Pattern_matches(pattern, line.buffer, current_line_number);
            line.buffer[match_length] = saved_character;
            if (matched < 0) {
                fprintf(stderr, "csplit: unable to evaluate pattern (out of memory)\n");
                goto failure;
            }

            if (matched > 0) {
                skip_line = pattern->type == PATTERN_REGEX_SKIP;
                if (pattern->offset < 0) {
                    unsigned long long distance =
                        (unsigned long long)(-(pattern->offset + 1)) + 1ULL;
                    if (distance >= current_line_number || distance > SIZE_MAX ||
                        !OutputLineQueue_mark_split(&queue,
                            current_line_number - (size_t)distance)) {
                        fprintf(stderr, "csplit: negative pattern offset is outside buffered input\n");
                        goto failure;
                    }
                } else if (pattern->offset > 0) {
                    unsigned long long distance = (unsigned long long)pattern->offset;
                    if (distance > SIZE_MAX - current_line_number) {
                        fprintf(stderr, "csplit: pattern offset exceeds supported line range\n");
                        goto failure;
                    }
                    pending_split_line = current_line_number + (size_t)distance;
                } else {
                    split_count++;
                }
                CSplitEngine_advance_pattern(pattern, &current_pattern_index,
                                            &repeat_counter);
            }
        }

        if (!OutputLineQueue_append(&queue, &line, current_line_number,
                                    skip_line, split_count)) {
            fprintf(stderr, "csplit: out of memory buffering input lines\n");
            goto failure;
        }
        while (queue.count > max_lookbehind) {
            if (!OutputLineQueue_write_oldest(&queue, output)) goto failure;
        }

        if (current_line_number == SIZE_MAX) {
            fprintf(stderr, "csplit: input contains too many lines\n");
            goto failure;
        }
        current_line_number++;
    }

    if (pending_split_line != 0) {
        fprintf(stderr, "csplit: pattern offset points beyond end of input\n");
        goto failure;
    }
    while (queue.count > 0) {
        if (!OutputLineQueue_write_oldest(&queue, output)) goto failure;
    }
    if (!OutputFileManager_close_current(output)) goto failure;

    DynamicString_destroy(&line);
    return true;

failure:
    OutputFileManager_rollback(output);
    OutputLineQueue_destroy(&queue);
    DynamicString_destroy(&line);
    return false;
}
