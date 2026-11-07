/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 * See the repository license file for the full license text.
 */

#define _CRT_SECURE_NO_WARNINGS
#include "output_file_manager.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct OutputFileManager {
    char prefix[256];
    char suffix_format[32];
    int digits;
    bool keep_files_on_error;
    bool quiet;
    bool elide_empty;

    size_t file_counter;
    FILE* current_file;
    size_t bytes_written_to_current;

    char** history;
    size_t history_count;
    size_t history_capacity;
};

bool OutputFileManager_validate_prefix(const char* prefix) {
    if (!prefix || !*prefix || strlen(prefix) >= 256) return false;
    for (const unsigned char* p = (const unsigned char*)prefix; *p; ++p) {
        if (*p < 32 || *p == 127 || *p == '/' || *p == '\\' || *p == ':' ||
            *p == '<' || *p == '>' || *p == '"' || *p == '|' || *p == '?' || *p == '*') {
            return false;
        }
    }
    return true;
}

bool OutputFileManager_validate_suffix_format(const char* format) {
    if (!format || !*format || strlen(format) >= 32) return false;
    const char* p = format;
    int conversions = 0;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                p++;
                continue;
            }
            if (*p == '0') p++;
            unsigned int width = 0;
            while (isdigit((unsigned char)*p)) {
                width = width * 10u + (unsigned int)(*p - '0');
                if (width > 64u) return false;
                p++;
            }
            if (*p != 'u' && *p != 'd' && *p != 'x' && *p != 'X') return false;
            conversions++;
            p++;
            continue;
        }
        if ((unsigned char)*p < 32 || *p == 127 || *p == '/' || *p == '\\' ||
            *p == ':' || *p == '<' || *p == '>' || *p == '"' || *p == '|' ||
            *p == '?' || *p == '*') {
            return false;
        }
        p++;
    }
    return conversions == 1;
}

static bool OutputFileManager_is_windows_device_name(const char* filename) {
#ifdef _WIN32
    const char* dot = strchr(filename, '.');
    size_t base_length = dot ? (size_t)(dot - filename) : strlen(filename);
    static const char* const names[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strlen(names[i]) != base_length) continue;
        size_t j = 0;
        while (j < base_length && tolower((unsigned char)filename[j]) ==
                                      tolower((unsigned char)names[i][j])) {
            j++;
        }
        if (j == base_length) return true;
    }
    size_t length = strlen(filename);
    return length > 0 && (filename[length - 1] == '.' || filename[length - 1] == ' ');
#else
    (void)filename;
    return false;
#endif
}

static bool OutputFileManager_add_history(OutputFileManager* manager, const char* filename) {
    if (manager->history_count >= manager->history_capacity) {
        size_t new_capacity = manager->history_capacity == 0 ? 16 : manager->history_capacity * 2;
        if (new_capacity < manager->history_capacity ||
            new_capacity > SIZE_MAX / sizeof(char*)) {
            return false;
        }
        char** new_history = (char**)realloc(manager->history,
                                              new_capacity * sizeof(char*));
        if (!new_history) return false;
        manager->history = new_history;
        manager->history_capacity = new_capacity;
    }
    size_t length = strlen(filename);
    if (length == SIZE_MAX || manager->history_count == SIZE_MAX) return false;
    char* copy = (char*)malloc(length + 1);
    if (!copy) return false;
    memcpy(copy, filename, length + 1);
    manager->history[manager->history_count++] = copy;
    return true;
}

static bool OutputFileManager_format_index(const OutputFileManager* manager,
                                           size_t index, char* output, size_t output_size) {
    if (index > INT_MAX || manager->suffix_format[0] == '\0') return false;
    char conversion = '\0';
    for (const char* p = manager->suffix_format; *p; ++p) {
        if (*p == '%' && p[1] == '%') {
            ++p;
        } else if (*p == '%') {
            while (p[1] && isdigit((unsigned char)p[1])) ++p;
            conversion = p[1];
            break;
        }
    }

    int length;
    if (conversion == 'u' || conversion == 'x' || conversion == 'X') {
        length = snprintf(output, output_size, manager->suffix_format, (unsigned int)index);
    } else {
        length = snprintf(output, output_size, manager->suffix_format, (int)index);
    }
    return length >= 0 && (size_t)length < output_size;
}

static bool OutputFileManager_close_current_internal(OutputFileManager* manager) {
    if (!manager->current_file) return true;

    bool success = true;
    if (fflush(manager->current_file) != 0) success = false;
    if (fclose(manager->current_file) != 0) success = false;
    manager->current_file = NULL;

    if (manager->elide_empty && manager->bytes_written_to_current == 0 &&
        manager->history_count > 0) {
        const char* last_file = manager->history[manager->history_count - 1];
        if (remove(last_file) != 0 && errno != ENOENT) {
            fprintf(stderr, "csplit: cannot remove empty output file '%s': %s\n",
                    last_file, strerror(errno));
            success = false;
        }
    } else if (!manager->quiet) {
        if (printf("%zu\n", manager->bytes_written_to_current) < 0 ||
            fflush(stdout) != 0) {
            success = false;
        }
    }
    manager->bytes_written_to_current = 0;
    if (!success) fprintf(stderr, "csplit: error finalizing output file\n");
    return success;
}

OutputFileManager* OutputFileManager_create(const char* prefix, int digits,
                                            const char* suffix_format, bool keep_files,
                                            bool quiet, bool elide_empty) {
    const char* actual_prefix = prefix ? prefix : "xx";
    if (!OutputFileManager_validate_prefix(actual_prefix) ||
        (suffix_format && !OutputFileManager_validate_suffix_format(suffix_format))) {
        return NULL;
    }

    OutputFileManager* manager = (OutputFileManager*)calloc(1, sizeof(OutputFileManager));
    if (!manager) return NULL;
    memcpy(manager->prefix, actual_prefix, strlen(actual_prefix) + 1);
    manager->digits = (digits > 0 && digits <= 64) ? digits : 2;
    manager->keep_files_on_error = keep_files;
    manager->quiet = quiet;
    manager->elide_empty = elide_empty;

    if (suffix_format) {
        memcpy(manager->suffix_format, suffix_format, strlen(suffix_format) + 1);
    } else {
        snprintf(manager->suffix_format, sizeof(manager->suffix_format), "%%0%dd",
                 manager->digits);
    }
    return manager;
}

bool OutputFileManager_open_next_file(OutputFileManager* manager) {
    if (!OutputFileManager_close_current_internal(manager)) return false;

    char formatted_index[128];
    char filename[sizeof(manager->prefix) + sizeof(manager->suffix_format) + 128];
    if (!OutputFileManager_format_index(manager, manager->file_counter,
                                        formatted_index, sizeof(formatted_index))) {
        fprintf(stderr, "csplit: cannot format output file suffix\n");
        return false;
    }
    int filename_length = snprintf(filename, sizeof(filename), "%s%s",
                                   manager->prefix, formatted_index);
    if (filename_length < 0 || (size_t)filename_length >= sizeof(filename)) {
        fprintf(stderr, "csplit: output filename is too long\n");
        return false;
    }
    if (OutputFileManager_is_windows_device_name(filename)) {
        fprintf(stderr, "csplit: refusing reserved output filename '%s'\n", filename);
        return false;
    }

    manager->current_file = fopen(filename, "wbx");
    if (!manager->current_file) {
        fprintf(stderr, "csplit: cannot open output file '%s': %s\n",
                filename, strerror(errno));
        return false;
    }
    if (!OutputFileManager_add_history(manager, filename)) {
        fclose(manager->current_file);
        manager->current_file = NULL;
        if (remove(filename) != 0 && errno != ENOENT) {
            fprintf(stderr, "csplit: cannot remove untracked output file '%s': %s\n",
                    filename, strerror(errno));
        }
        fprintf(stderr, "csplit: out of memory tracking output files\n");
        return false;
    }

    manager->file_counter++;
    manager->bytes_written_to_current = 0;
    return true;
}

bool OutputFileManager_write_line(OutputFileManager* manager,
                                  const char* line, size_t length) {
    if (!manager->current_file && !OutputFileManager_open_next_file(manager)) return false;
    if (length > SIZE_MAX - manager->bytes_written_to_current ||
        fwrite(line, 1, length, manager->current_file) != length) {
        fprintf(stderr, "csplit: error writing output file: %s\n", strerror(errno));
        return false;
    }
    manager->bytes_written_to_current += length;
    return true;
}

bool OutputFileManager_close_current(OutputFileManager* manager) {
    return OutputFileManager_close_current_internal(manager);
}

void OutputFileManager_rollback(OutputFileManager* manager) {
    if (manager->current_file) {
        if (fclose(manager->current_file) != 0) {
            fprintf(stderr, "csplit: error closing output during rollback: %s\n",
                    strerror(errno));
        }
        manager->current_file = NULL;
    }
    if (!manager->keep_files_on_error) {
        for (size_t i = 0; i < manager->history_count; ++i) {
            if (manager->history[i] && remove(manager->history[i]) != 0 && errno != ENOENT) {
                fprintf(stderr, "csplit: cannot remove output file '%s' during rollback: %s\n",
                        manager->history[i], strerror(errno));
            }
        }
    }
}

void OutputFileManager_destroy(OutputFileManager* manager) {
    if (!manager) return;
    OutputFileManager_close_current_internal(manager);
    for (size_t i = 0; i < manager->history_count; ++i) {
        free(manager->history[i]);
    }
    free(manager->history);
    free(manager);
}
